/*
 * consumer.c - Consumer side of the bounded-buffer problem.
 *
 * Attaches to the shared ring buffer created by the producer (waiting politely
 * if the producer has not started yet), then consumes one item per 1/rate
 * seconds, blocking whenever the buffer is empty. Ctrl+C shuts it down cleanly.
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    bb_install_signal_handlers();

    int rate = bb_prompt_int("Enter the consuming rate (items/sec): ",
                             1, BB_MAX_RATE);

    bb_shared_t *sh;
    int shmid = bb_shm_open_wait(&sh);
    if (shmid == -1) {
        printf("\nStopped before the buffer was ready.\n");
        return 0;
    }
    int semid = bb_sem_open();

    printf("Consumer ready: capacity=%d, rate=%d/s. Press Ctrl+C to stop.\n",
           sh->capacity, rate);

    while (!g_stop) {
        bb_sleep_per_rate(rate);
        if (g_stop)
            break;

        if (bb_sem_op(semid, SEM_FULL, -1) == -1)   /* wait for an item */
            break;
        if (bb_sem_op(semid, SEM_MUTEX, -1) == -1) { /* enter critical section */
            bb_sem_op(semid, SEM_FULL, +1);
            break;
        }

        int slot = sh->out;
        int item = sh->buffer[slot];
        sh->out = (sh->out + 1) % sh->capacity;

        bb_sem_op(semid, SEM_MUTEX, +1);             /* leave critical section */
        bb_sem_op(semid, SEM_EMPTY, +1);             /* signal a free slot     */

        printf("Consumed item %d <- slot %d\n", item, slot);
    }

    printf("\nConsumer shutting down; releasing IPC resources.\n");
    bb_shm_remove(shmid, sh);
    bb_sem_remove(semid);
    return 0;
}
