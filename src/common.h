/*
 * common.h - Shared definitions for the Bounded Buffer (producer/consumer) demo.
 *
 * The producer and consumer are separate processes that cooperate over System V
 * shared memory (the ring buffer) and a System V semaphore set (empty/full/mutex).
 * Everything they must agree on - IPC keys, the shared layout, and the helper API -
 * lives here so both programs stay in sync.
 */
#ifndef BOUNDED_BUFFER_COMMON_H
#define BOUNDED_BUFFER_COMMON_H

#include <sys/types.h>   /* key_t   */
#include <stddef.h>      /* size_t  */
#include <signal.h>      /* sig_atomic_t */

/* ---- IPC keys (must match between producer and consumer) ---- */
#define BB_SHM_KEY  ((key_t)0x00424201)  /* shared memory: control block + ring buffer */
#define BB_SEM_KEY  ((key_t)0x00424202)  /* semaphore set: empty, full, mutex          */

/* ---- Semaphore indices within the single semaphore set ---- */
enum { SEM_EMPTY = 0, SEM_FULL = 1, SEM_MUTEX = 2, SEM_COUNT = 3 };

/* ---- Buffer / rate limits ---- */
#define BB_MIN_CAPACITY 1
#define BB_MAX_CAPACITY 1024
#define BB_MAX_RATE     1000

/*
 * Written last by the producer once the shared state is fully initialized, so a
 * consumer that happens to start first can wait until the buffer is ready.
 */
#define BB_MAGIC 0x42427631u  /* 'B','B','v','1' */

/*
 * Shared control block. Lives at the start of the shared memory segment and is
 * immediately followed by `capacity` ints (the ring buffer itself).
 */
typedef struct {
    volatile unsigned magic;  /* BB_MAGIC once fully initialized */
    int capacity;             /* number of slots in buffer[]     */
    int in;                   /* next index the producer writes  */
    int out;                  /* next index the consumer reads   */
    int buffer[];             /* flexible array member: `capacity` ints */
} bb_shared_t;

/* Set by the SIGINT/SIGTERM handler; polled by the main loops for a clean exit. */
extern volatile sig_atomic_t g_stop;

/* ---- Process/setup helpers (defined in common.c) ---- */

void   bb_die(const char *msg);            /* perror(msg) then exit(EXIT_FAILURE) */
void   bb_install_signal_handlers(void);   /* SIGINT/SIGTERM -> g_stop = 1        */
int    bb_prompt_int(const char *prompt, int lo, int hi);  /* validated stdin int */
void   bb_sleep_per_rate(int rate_per_sec);               /* sleep 1/rate seconds */
size_t bb_shared_size(int capacity);       /* bytes for header + capacity ints    */

/* ---- Semaphore helpers ---- */

/*
 * Perform one semaphore operation (blocking).
 * Returns 0 on success, or -1 if the wait was interrupted by a signal (EINTR) or
 * the set was removed by the peer (EIDRM) - both meaning "time to stop". Any other
 * error is fatal (aborts the process).
 */
int  bb_sem_op(int semid, int semnum, int delta);

int  bb_sem_create(int capacity);  /* producer: create + initialize the set   */
int  bb_sem_open(void);            /* consumer: open the existing set          */
void bb_sem_remove(int semid);     /* IPC_RMID, tolerating "already removed"   */

/* ---- Shared-memory helpers ---- */

int  bb_shm_create(int capacity, bb_shared_t **out);  /* producer: create + attach + init */
int  bb_shm_open_wait(bb_shared_t **out);             /* consumer: wait for producer, attach */
void bb_shm_remove(int shmid, bb_shared_t *addr);     /* detach + IPC_RMID, tolerant */

#endif /* BOUNDED_BUFFER_COMMON_H */
