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

///////   /////////////  \\\\\\/
///////   G L O B A L S  \\\\\\/
///////   /////////////  \\\\\\/
int rcvfromServer = 0;
int isExit = 0;


struct sharedMemory { 

    char buff[100]; 
    int clientpid, serverpid; 
}; 

struct sharedMemory* shmaddr; 


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


void handler(int signum){

    if (signum == SIGUSR2){

        printf("\nThe server has finished processing tha data, NEW DATA IS   = %s\n",shmaddr->buff);
        rcvfromServer = 1;
        
    }
}

void exitHandler(int signum){
    
    isExit = 1;
}
    

        
int main()
{
    // Client code
    pid_t cliPID = getpid();
    int shmid;
    key_t key = 5000;


    // S E M A P H O R E S 
    union Semun semun;

    int sem1 = create_sem(4000);

    struct sembuf DOWNSemapohre;
    struct sembuf UPSemapohre;


             ////                               ///////   
             ////   S H A R E D M E M O R Y     ///////                     
             ////                               ///////  

    shmid = shmget(key, sizeof(struct sharedMemory), IPC_CREAT|0644);

    if(shmid == -1){
        perror("Error in create");
        exit(-1);
    }

    else{
        printf("\nShared memory -- Client ID = %d\n", shmid);

    }
    
    
    // attach the Cleint segment to our space
    shmaddr = (struct sharedMemory*) shmat(shmid, (void *)0, 0);
      if(shmaddr == -1)
    {
        perror("Error in attach in reader");
        exit(-1);   
    }
    printf("\nShared memory -- Client attached at address %x\n", shmaddr);


    // declaring the signal used by the client to inform the server that the client has write a msg 
    // SIGUSR2 >>> Client
    signal(SIGUSR2, handler);
    signal(SIGINT,exitHandler);

    printf("Enter your message: ");

    while(isExit ==0){

        /// MULTIPLE C L I E N T HANDLING //
        
        DOWNSemapohre = down(sem1);
        //printf("Enter your message: \n");
        shmaddr->clientpid = cliPID;

        scanf("%s", shmaddr->buff);
        kill(shmaddr->serverpid , SIGUSR1);


        while (rcvfromServer != 1){}
        rcvfromServer = 0;
        UPSemapohre = up(sem1);
        printf("Enter your message: ");
      
    }

    // detach client
    shmdt((void*)shmaddr);
    // clear resources
    destroy_sem(sem1);
    shmctl(shmid, IPC_RMID, NULL);
    
    exit(0);
    
    return 0;
}
