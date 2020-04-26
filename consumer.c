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



///////   /////////////  \\\\\\/
///////   G L O B A L S  \\\\\\/
///////   /////////////  \\\\\\/

int consIndexLoc = 0;
int isExit = 0;





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

int create_sem(int key)
{
    union Semun semun;

    int sem = semget(key, 1, 0666|IPC_CREAT);

    if(sem == -1)
    {
        perror("Error in create sem");
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


int removeItem(int* ptrBuff, int buffLen){

    int item;
    if (ptrBuff[consIndexLoc] != -100){
        item = ptrBuff[consIndexLoc];
        ptrBuff[consIndexLoc] = -100;
        printf("\nThe Removed Item is %d\n",item);

    }
    if (consIndexLoc < buffLen){
        consIndexLoc++;
    }
    else{
        consIndexLoc = 0;
    }
        return item;
}

void consumeItem(int item){
    printf("\nThe item consumed is %d\n",item);
}

void exitHandler(int signum){
    
    isExit = 1;
}
int main(){

    // RATE of PRODUCTION
    int rate;
    printf("\nEnter the consuming rate: ");
    scanf("%d",&rate);

    // Shared memory of size
    int shmidCreation; // id of the shared memory for the size of the buffer
    key_t keyCreation = 4000;

    shmidCreation = shmget(keyCreation, sizeof(int), IPC_CREAT|0644);

    if(shmidCreation == -1){
        perror("Error in create");
        exit(-1);
    }

    else{
        printf("\nShared memory -- Consumer ID = %d\n", shmidCreation);
    }

    // Attach to space segment
    int* shmaddrCreation = (int*) shmat(shmidCreation, (void *)0, 0);  

      if(shmaddrCreation == -1)
    {
        perror("Error in attach in size creation");
        exit(-1);
    }
    // E N D 


    int LEN = *shmaddrCreation;
    int size;
    int* buff;
    size = sizeof(int) * LEN;

    
            ////                               ///////   
            ////   S H A R E D M E M O R Y     ///////                     
            ////                               ///////  
    
    int shmid;
    key_t key = 5000;

    shmid = shmget(key, size, IPC_CREAT|0644);

    if(shmid == -1){
        perror("Error in create");
        exit(-1);
    }

    else{
        printf("\nShared memory -- Consumer ID = %d\n", shmid);
    }
    
   
    // attach the Server segment to our space

    buff = (int*) shmat(shmid, (void *)0, 0);
      if(buff == -1)
    {
        perror("Error in attach in reader");
        exit(-1);
    }
    
    printf("\nShared memory -- Consumer attached at address %x\n", buff);

    
    // S E M A P H O R E S 

    int full = create_sem(100);
    int empty = create_sem(200);
    int mutex = create_sem(300);

    signal(SIGINT,exitHandler);
    
    while (isExit == 0){
        
        sleep(1/rate);
        down(full);
        down(mutex);
        int item = removeItem(buff,LEN);
        up(mutex);
        up(empty);
        consumeItem(item);
    }
    // detach consumer
    shmdt((void*)buff);
    shmdt((void*)shmaddrCreation);

    // clear resources
    destroy_sem(full);
    destroy_sem(empty);
    destroy_sem(mutex);

    shmctl(shmid, IPC_RMID, NULL);
    shmctl(shmidCreation, IPC_RMID, NULL);
    
    exit(0);
}
    

