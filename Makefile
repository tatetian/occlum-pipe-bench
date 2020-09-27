CC := occlum-gcc
CFLAGS := -O2

.PHONY: all clean

all: bin bin/main bin/sink

bin:
	mkdir -p bin

bin/main: main.c
	$(CC) $(CFLAGS) main.c -o bin/main

bin/sink: sink.c
	$(CC) $(CFLAGS) sink.c -o bin/sink

TOTAL_DATA_GB = 16
BUF_SIZE_KB = 4

bench: bin/main bin/sink
	./bin/main $(TOTAL_DATA_GB) $(BUF_SIZE_KB)

bench-occlum: bin/main bin/sink occlum
	cp bin/main bin/sink occlum/image/bin/
	cd occlum && \
		occlum build && \
		occlum run /bin/main $(TOTAL_DATA_GB) $(BUF_SIZE_KB)

occlum:
	occlum new occlum
	new_json=`jq '.process.default_mmap_size = "32MB"' occlum/Occlum.json` && \
		echo "$$new_json" > occlum/Occlum.json

clean:
	$(RM) -rf bin occlum
