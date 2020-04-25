#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <sys/shm.h>
#include <signal.h> 
#include <sys/sem.h>

#include <pthread.h>

struct sharedMemory { 

    char buff[100]; 
    int clientpid, serverpid; 
}; 

struct sharedMemory* shmaddr; 

#define N 100





             ////                            ///////   
             ////    S E M A P H O R E S     ///////                     
             ////                            ///////  

/* arg for semctl system calls. */
union Semun
{
    int val;                /* value for SETVAL */
    struct semid_ds *buf;   /* buffer for IPC_STAT & IPC_SET */
    ushort *array;          /* array for GETALL & SETALL */
    struct seminfo *__buf;  /* buffer for IPC_INFO */
    void *__pad;
};

int create_sem(int key, int initial_value)
{
    union Semun semun;

    int sem = semget(key, 1, 0666|IPC_CREAT);

    if(sem == -1)
    {
        perror("Error in create sem");
        exit(-1);
    }

    semun.val = initial_value;  /* initial value of the semaphore, Binary semaphore */
    if(semctl(sem, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }
    
    return sem;
}

void destroy_sem(int sem)
{
    if(semctl(sem, 0, IPC_RMID) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }
}

struct sembuf down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if(semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
    return p_op;
}

struct sembuf up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if(semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
    return v_op;
}




int main(){


    struct sharedMemory { 

    char buff[100]; 
    int clientpid, serverpid; 
    }
}

struct sharedMemory* shmaddr; 


    int size;
    printf("\nEnter the buffer size: ");
    scanf("%d",&size);
    int * myBuffer = (int *) malloc(sizeof(int));

    int shmid;
    key_t key = 5000;



             ////                               ///////   
             ////   S H A R E D M E M O R Y     ///////                     
             ////                               ///////  

    shmid = shmget(key, sizeof(myBuffer), IPC_CREAT|0644);

    if(shmid == -1){
        perror("Error in create");
        exit(-1);
    }

    else{
        printf("\nShared memory -- Server ID = %d\n", shmid);
    }
    
    
    // attach the Server segment to our space
    shmaddr = (struct sharedMemory*) shmat(shmid, (void *)0, 0);
      if(shmaddr == -1)
    {
        perror("Error in attach in reader");
        exit(-1);
    }
    printf("\nShared memory -- Server attached at address %x\n", shmaddr);
    

    pthread_mutex_t mutex;

    // S E M A P H O R E S 
    union Semun semun;

    int full = create_sem(4000,0);
    int empty = create_sem(4000,N);

    struct sembuf DOWNSemapohre;
    struct sembuf UPSemapohre;

    while(1){

        int item = produceItem();
        down(empty);
        pthread_mutex_lock(&mutex);
        insertItem(item);
        pthread_mutex_unlock(&mutex);
        up(full);
    }

}
    

