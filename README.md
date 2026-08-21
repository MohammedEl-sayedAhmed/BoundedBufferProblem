# Bounded Buffer — Producer / Consumer

A classic **bounded-buffer (producer/consumer)** synchronization demo in C, using
**System V IPC**: a shared-memory ring buffer coordinated by three System V
semaphores (`empty`, `full`, `mutex`). The producer and consumer are two separate
processes that communicate entirely through the shared segment.

## How it works

The shared memory holds a small control block followed by the ring buffer:

```
+---------+----------+-----+------+---------------------------+
| magic   | capacity | in  | out  | buffer[0] .. buffer[N-1]  |
+---------+----------+-----+------+---------------------------+
```

Synchronization follows the textbook counting-semaphore solution:

| Semaphore | Initial value | Meaning                        |
|-----------|---------------|--------------------------------|
| `empty`   | `capacity`    | number of free slots           |
| `full`    | `0`           | number of filled slots         |
| `mutex`   | `1`           | mutual exclusion for the buffer |

- **Producer:** `down(empty) → down(mutex) → write slot → up(mutex) → up(full)`
- **Consumer:** `down(full) → down(mutex) → read slot → up(mutex) → up(empty)`

Because the counting semaphores guarantee a slot is free before writing and filled
before reading, the buffer indices simply advance modulo `capacity` — no sentinel
values and no out-of-bounds accesses.

## Features

- **Order-independent startup** — start the producer or the consumer first. If the
  consumer starts first, it waits until the producer has created and initialized the
  buffer.
- **Graceful shutdown** — `Ctrl+C` (SIGINT/SIGTERM) stops each process cleanly and
  releases its IPC resources; a blocked `semop()` is interrupted rather than left hanging.
- **Reliable cleanup** — shared memory and semaphores are removed on exit, tolerant of
  the peer having already removed them. `SEM_UNDO` on the mutex prevents a `kill -9`
  in the critical section from deadlocking the other process.
- **Real rate limiting** — production/consumption rate is honored down to sub-second
  intervals (`nanosleep(1/rate)`), with input validation.

## Project layout

```
.
├── src/
│   ├── common.h      # shared layout, IPC keys, helper API
│   ├── common.c      # IPC/semaphore/signal helpers
│   ├── producer.c    # producer process
│   └── consumer.c    # consumer process
├── docs/             # assignment PDFs
├── Makefile
├── README.md
├── LICENSE
└── .gitignore
```

## Build

```sh
make          # builds ./producer and ./consumer
```

Requires `gcc` and a Linux system with System V IPC (standard on Linux).

## Run

Use **two terminals** — both programs are long-running and run concurrently:

```sh
# Terminal 1
./producer
Enter the buffer size: 5
Enter the production rate (items/sec): 2

# Terminal 2
./consumer
Enter the consuming rate (items/sec): 1
```

Each side prints the items it produces/consumes. Stop either process with `Ctrl+C`;
it detaches and releases its IPC resources on the way out.

## Cleaning up

```sh
make clean       # remove the compiled binaries
make clean-ipc   # remove any leftover shared memory / semaphores
```

`make clean-ipc` is only needed if a process was killed with `kill -9` (which bypasses
the normal cleanup). Inspect leftover IPC with `ipcs -m` (shared memory) and `ipcs -s`
(semaphores).
