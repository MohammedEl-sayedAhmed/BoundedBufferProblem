/*
 * common.c - Shared helpers for the producer and consumer.
 *
 * Built with -std=gnu11, which exposes the System V IPC and POSIX signal APIs
 * without extra feature-test macros.
 */
#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

volatile sig_atomic_t g_stop = 0;

/* glibc does NOT define `union semun`; callers of semctl(...SETVAL...) must. */
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

/* ------------------------------------------------------------------ */
/* Process / setup helpers                                            */
/* ------------------------------------------------------------------ */

void bb_die(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

void bb_install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  /* deliberately NO SA_RESTART: interrupt a blocked semop()
                         so the main loop can notice g_stop and exit cleanly. */

    if (sigaction(SIGINT, &sa, NULL) == -1)  bb_die("sigaction(SIGINT)");
    if (sigaction(SIGTERM, &sa, NULL) == -1) bb_die("sigaction(SIGTERM)");
}

int bb_prompt_int(const char *prompt, int lo, int hi)
{
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);

        int v;
        int n = scanf("%d", &v);
        if (n == 1 && v >= lo && v <= hi)
            return v;

        if (n == EOF) {
            fprintf(stderr, "\nInput closed; aborting.\n");
            exit(EXIT_FAILURE);
        }

        /* Discard the rest of the offending line before re-prompting. */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
        fprintf(stderr, "Please enter an integer in [%d, %d].\n", lo, hi);
    }
}

void bb_sleep_per_rate(int rate_per_sec)
{
    if (rate_per_sec <= 0)
        rate_per_sec = 1;

    long ns = 1000000000L / rate_per_sec;
    struct timespec req = { .tv_sec = ns / 1000000000L, .tv_nsec = ns % 1000000000L };

    /* If a signal interrupts the sleep we simply return early; the caller
       re-checks g_stop right after. */
    nanosleep(&req, NULL);
}

size_t bb_shared_size(int capacity)
{
    return sizeof(bb_shared_t) + (size_t)capacity * sizeof(int);
}

/* ------------------------------------------------------------------ */
/* Semaphore helpers                                                  */
/* ------------------------------------------------------------------ */

int bb_sem_op(int semid, int semnum, int delta)
{
    struct sembuf op;
    op.sem_num = (unsigned short)semnum;
    op.sem_op  = delta;
    /* Blocking wait. SEM_UNDO only on the mutex: if a process is killed while
       holding it, the kernel auto-releases it so the peer can't deadlock. Undo
       must NOT be used on the empty/full counters, where it would reverse
       already-produced/consumed accounting on exit. */
    op.sem_flg = (semnum == SEM_MUTEX) ? SEM_UNDO : 0;

    while (semop(semid, &op, 1) == -1) {
        if (errno == EINTR || errno == EIDRM)
            return -1;              /* interrupted, or peer removed the set */
        bb_die("semop");
    }
    return 0;
}

int bb_sem_create(int capacity)
{
    int semid = semget(BB_SEM_KEY, SEM_COUNT, IPC_CREAT | 0666);
    if (semid == -1)
        bb_die("semget");

    union semun arg;
    arg.val = capacity; if (semctl(semid, SEM_EMPTY, SETVAL, arg) == -1) bb_die("semctl(EMPTY)");
    arg.val = 0;        if (semctl(semid, SEM_FULL,  SETVAL, arg) == -1) bb_die("semctl(FULL)");
    arg.val = 1;        if (semctl(semid, SEM_MUTEX, SETVAL, arg) == -1) bb_die("semctl(MUTEX)");

    return semid;
}

int bb_sem_open(void)
{
    int semid = semget(BB_SEM_KEY, SEM_COUNT, 0666);
    if (semid == -1)
        bb_die("semget");
    return semid;
}

void bb_sem_remove(int semid)
{
    if (semctl(semid, 0, IPC_RMID) == -1 && errno != EINVAL && errno != EIDRM)
        perror("semctl(IPC_RMID)");
}

/* ------------------------------------------------------------------ */
/* Shared-memory helpers                                              */
/* ------------------------------------------------------------------ */

int bb_shm_create(int capacity, bb_shared_t **out)
{
    int shmid = shmget(BB_SHM_KEY, bb_shared_size(capacity), IPC_CREAT | 0666);
    if (shmid == -1) {
        if (errno == EINVAL)
            fprintf(stderr,
                    "A stale buffer of a different size may already exist.\n"
                    "Run 'make clean-ipc' to clear leftover IPC, then retry.\n");
        bb_die("shmget");
    }

    bb_shared_t *sh = shmat(shmid, NULL, 0);
    if (sh == (void *)-1)
        bb_die("shmat");

    sh->capacity = capacity;
    sh->in = 0;
    sh->out = 0;
    for (int i = 0; i < capacity; i++)
        sh->buffer[i] = 0;

    sh->magic = BB_MAGIC;   /* publish last: signals the consumer we are ready */

    *out = sh;
    return shmid;
}

int bb_shm_open_wait(bb_shared_t **out)
{
    int shmid;
    int announced = 0;

    /* Wait for the producer to create the segment (size 0 = any existing size). */
    while ((shmid = shmget(BB_SHM_KEY, 0, 0666)) == -1) {
        if (errno != ENOENT)
            bb_die("shmget");
        if (g_stop)
            return -1;
        if (!announced) {
            printf("Waiting for the producer to create the buffer...\n");
            announced = 1;
        }
        struct timespec t = { .tv_sec = 0, .tv_nsec = 100000000L };  /* 100 ms */
        nanosleep(&t, NULL);
    }

    bb_shared_t *sh = shmat(shmid, NULL, 0);
    if (sh == (void *)-1)
        bb_die("shmat");

    /* Wait until the producer has finished initializing the control block. */
    while (sh->magic != BB_MAGIC) {
        if (g_stop) {
            shmdt(sh);
            return -1;
        }
        struct timespec t = { .tv_sec = 0, .tv_nsec = 50000000L };   /* 50 ms */
        nanosleep(&t, NULL);
    }

    *out = sh;
    return shmid;
}

void bb_shm_remove(int shmid, bb_shared_t *addr)
{
    if (addr != NULL && shmdt(addr) == -1)
        perror("shmdt");

    /* IPC_RMID marks the segment for deletion once the last process detaches, so
       it is safe for both producer and consumer to call it. */
    if (shmctl(shmid, IPC_RMID, NULL) == -1 && errno != EINVAL && errno != EIDRM)
        perror("shmctl(IPC_RMID)");
}
