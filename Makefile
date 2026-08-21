# Makefile for the Bounded Buffer (producer/consumer) demo.

CC      := gcc
CFLAGS  := -std=gnu11 -Wall -Wextra -O2
SRC     := src
COMMON  := $(SRC)/common.c $(SRC)/common.h
BINS    := producer consumer

# Known IPC keys (kept in sync with src/common.h) for the clean-ipc target.
SHM_KEY := 0x00424201
SEM_KEY := 0x00424202

.PHONY: all clean clean-ipc run-producer run-consumer

all: $(BINS)

producer: $(SRC)/producer.c $(COMMON)
	$(CC) $(CFLAGS) $(SRC)/producer.c $(SRC)/common.c -o $@

consumer: $(SRC)/consumer.c $(COMMON)
	$(CC) $(CFLAGS) $(SRC)/consumer.c $(SRC)/common.c -o $@

# Convenience targets: run each in its own terminal (producer first, or either;
# the consumer waits for the producer if it starts first).
run-producer: producer
	./producer

run-consumer: consumer
	./consumer

clean:
	rm -f $(BINS)

# Remove any shared memory / semaphores left over from an interrupted run.
# The leading '-' lets make ignore "not found" errors.
clean-ipc:
	-ipcrm -M $(SHM_KEY) 2>/dev/null
	-ipcrm -S $(SEM_KEY) 2>/dev/null
