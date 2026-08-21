/*
 * producer.c - Producer side of the bounded-buffer problem.
 *
 * Creates the shared ring buffer and the empty/full/mutex semaphores, then
 * produces a stream of sequential integers, one per 1/rate seconds, blocking
 * whenever the buffer is full. Ctrl+C shuts it down cleanly and releases IPC.
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    bb_install_signal_handlers();

    int capacity = bb_prompt_int("Enter the buffer size: ",
                                 BB_MIN_CAPACITY, BB_MAX_CAPACITY);
    int rate     = bb_prompt_int("Enter the production rate (items/sec): ",
                                 1, BB_MAX_RATE);

    /* Create the semaphores BEFORE publishing the shared memory, so a consumer
       that keys off the buffer's "ready" flag always finds the semaphores. */
    int semid = bb_sem_create(capacity);

    bb_shared_t *sh;
    int shmid = bb_shm_create(capacity, &sh);

    printf("Producer ready: capacity=%d, rate=%d/s. Press Ctrl+C to stop.\n",
           capacity, rate);

    int item = 0;
    while (!g_stop) {
        bb_sleep_per_rate(rate);
        if (g_stop)
            break;

        if (bb_sem_op(semid, SEM_EMPTY, -1) == -1)  /* wait for a free slot */
            break;
        if (bb_sem_op(semid, SEM_MUTEX, -1) == -1) { /* enter critical section */
            bb_sem_op(semid, SEM_EMPTY, +1);
            break;
        }

        int slot = sh->in;
        sh->buffer[slot] = ++item;
        sh->in = (sh->in + 1) % sh->capacity;

        bb_sem_op(semid, SEM_MUTEX, +1);             /* leave critical section */
        bb_sem_op(semid, SEM_FULL, +1);              /* signal item available  */

        printf("Produced item %d -> slot %d\n", item, slot);
    }

    printf("\nProducer shutting down; releasing IPC resources.\n");
    bb_shm_remove(shmid, sh);
    bb_sem_remove(semid);
    return 0;
}
