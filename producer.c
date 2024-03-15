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
#include <semaphore.h>




///////   /////////////  \\\\\\/
///////   G L O B A L S  \\\\\\/
///////   /////////////  \\\\\\/

int prodIndexLoc = 0;
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


int produceItem(int* ptrBuff, int buffLen){

    printf("\nProducer tries to produce an item\n");

    int count = 0;
    for(int i = 0; i < buffLen; i++){
        
        if (ptrBuff[i] == -100){
            count++;
        }

    }
    return count;
}

void insertItem(int item,int* ptrBuff, int buffLen){

    if (ptrBuff[prodIndexLoc == -100]){
        ptrBuff[prodIndexLoc] = item;
        printf("\nInserted item is %d\n",item);

    }
    

    if ( prodIndexLoc < buffLen ){
        prodIndexLoc++;
    }
    else{
        prodIndexLoc = 0;
    }

}

void exitHandler(int signum){
    
    isExit = 1;
}

int main(){


    /// S I Z E I D E N TI FI C A T I O N -- S H A R E D M E M O R Y 



    int LEN; // length of the buffer array - to be created
    int size; // size of memory allocated
    printf("\nEnter the buffer size: ");
    scanf("%d",&LEN);
    LEN--;
    int* buff;
    size = sizeof(int) * LEN;

    // RATE of PRODUCTION
    int rate;
    printf("\nEnter the production rate: ");
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
        printf("\nShared memory -- Producer size ID = %d\n", shmidCreation);
    }

    // Attach to space segment
    int* shmaddrCreation = (int*) shmat(shmidCreation, (void *)0, 0);  

      if(shmaddrCreation == -1)
    {
        perror("Error in attach in size creation");
        exit(-1);
    }
    *shmaddrCreation = LEN;
    // E N D 



    
             ////                                            ///////   
             ////   B U F F E R  S H A R E D M E M O R Y     ///////                     
             ////                                            ///////  
    
    int shmid;
    key_t key = 5000;

    shmid = shmget(key, size, IPC_CREAT|0644);

    if(shmid == -1){
        perror("Error in create");
        exit(-1);
    }

    else{
        printf("\nShared memory -- Producer ID = %d\n", shmid);
    }
    
   
    // attach the Server segment to our space

    buff = (int*) shmat(shmid, (void *)0, 0);
      if(buff == -1)
    {
        perror("Error in attach in Producer");
        exit(-1);
    }
    printf("\nShared memory -- Producer attached at address %x\n", buff);


    // buff initilization with -100 -- test condition

    for (int i =0; i < LEN; i++){
        buff[i] = -100;
    }


    // S E M A P H O R E S 

    int full = create_sem(100,0);
    int empty = create_sem(200,LEN);
    int mutex = create_sem(300,1);

    signal(SIGINT,exitHandler);

    while(isExit == 0){
        
        sleep(1/rate);
        int item = produceItem(buff,LEN);
        down(empty);
        down(mutex);
        insertItem(item,buff,LEN);
        up(mutex);       
        up(full); 
    }  
    // detach producer
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
    

