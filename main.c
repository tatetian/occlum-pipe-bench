#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <spawn.h>
#include <string.h>

#define KB              (1024UL)
#define MB              (1024UL * 1024UL)
#define GB              (1024UL * 1024UL * 1024UL)

#define TOTAL_BYTES     (2 * GB)
#define MAX_BUF_SIZE    (128 * KB)

#define MIN(x, y)       ((x) <= (y) ? (x) : (y))

static void show_help(const char* prog_name, const char* error_msg) {
    printf("Error: %s\n\n", error_msg);
    printf("Usage: %s <total_data_in_gb> <buf_size_in_kb>\n", prog_name);
}

int main(int argc, const char *argv[]) {
    if (argc != 3) {
        show_help(argv[0], "wrong number of arguments");
        return -1;
    }

    char* endptr = NULL;
    const char* remain_bytes_str = argv[1];
    if (strlen(remain_bytes_str) == 0) {
        show_help(argv[0], "the first argument is empty");
        return -1;
    }
    long val = strtol(remain_bytes_str, &endptr, 10);
    if (*endptr != '\0') {
        show_help(argv[0], "the first argument is not a valid integer");
        return -1;
    }
    if (val <= 0) {
        show_help(argv[0], "the first argument must be positive");
        return -1;
    }
    size_t remain_bytes = (size_t)val * GB;

    const char* buf_size_str = argv[1];
    if (strlen(buf_size_str) == 0) {
        show_help(argv[1], "the second argument is empty");
        return -1;
    }
    val = strtol(buf_size_str, &endptr, 10);
    if (*endptr != '\0') {
        show_help(argv[1], "the second argument is not a valid integer");
        return -1;
    }
    if (val <= 0) {
        show_help(argv[1], "the second argument must be positive");
        return -1;
    }
    size_t buf_size = (size_t)val * KB;
    if (buf_size > MAX_BUF_SIZE) {
        show_help(argv[1], "the second argument must be too large");
        return -1;
    }

    printf("Total bytes to transfer: %lu bytes\n", remain_bytes);
    printf("Buffer size: %lu bytes\n", buf_size);

    // Create pipe
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        printf("ERROR: failed to create a pipe\n");
        return -1;
    }
    int pipe_rd_fd = pipe_fds[0];
    int pipe_wr_fd = pipe_fds[1];

    // Spawn a child process that reads from the pipe
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    posix_spawn_file_actions_adddup2(&file_actions, pipe_rd_fd, STDIN_FILENO);
    posix_spawn_file_actions_addclose(&file_actions, pipe_wr_fd);

    char* args[2] = { "sink", NULL };
    int child_pid;
    if (posix_spawn(&child_pid, "./bin/sink", &file_actions,
                    NULL, args, NULL) < 0) {
        printf("ERROR: failed to spawn a child process\n");
        return -1;
    }
    close(pipe_rd_fd);

    // Start the timer
    struct timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);

    // Tell the reader how many data are to be transfered
    if (write(pipe_wr_fd, &remain_bytes, sizeof(remain_bytes)) != sizeof(remain_bytes)) {
        printf("ERROR: failed to write to pipe\n");
        return -1;
    }

    // Tell the reader the buffer size that it should use
    if (write(pipe_wr_fd, &buf_size, sizeof(buf_size)) != sizeof(buf_size)) {
        printf("ERROR: failed to write to pipe\n");
        return -1;
    }

    // Write a specified amount of data in a buffer of specified size
    char buf[MAX_BUF_SIZE] = {0};
    while (remain_bytes > 0) {
        size_t len = MIN(buf_size, remain_bytes);
        if ((len = write(pipe_wr_fd, buf, len)) < 0) {
            printf("ERROR: failed to write to pipe\n");
            return -1;
        }
        remain_bytes -= len;
    }

    // Wait for the child process to read all data and exit
    int status = 0;
    if (wait4(child_pid, &status, 0, NULL) < 0) {
        printf("ERROR: failed to wait4 the child process\n");
        return -1;
    }

    // Stop the timer
    gettimeofday(&tv_end, NULL);

    // Calculate the throughput
    double total_s = (tv_end.tv_sec - tv_start.tv_sec)
                     + (double)(tv_end.tv_usec - tv_start.tv_usec) / 1000000;
    if (total_s < 1.0) {
        printf("WARNING: run long enough to get meaningful results\n");
        if (total_s == 0) { return 0; }
    }
    double total_mb = (double)TOTAL_BYTES / MB;
    double throughput = total_mb / total_s;
    printf("Throughput of pipe is %.2f MB/s\n", throughput);
    return 0;
}
