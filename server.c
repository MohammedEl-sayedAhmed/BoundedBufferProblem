#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/shm.h>
#include <sys/sem.h>

///////   /////////////  \\\\\\/
///////   G L O B A L S  \\\\\\/
///////   /////////////  \\\\\\/

int rcvfromClient = 0;
int isExit = 0;


struct sharedMemory { 

    char buff[100]; 
    pid_t clientpid, serverpid; 
}; 

struct sharedMemory* shmaddr = NULL;

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


/* convert upper case to lower case or vise versa */
void conv(char *msg)
{
    int size = strlen(msg);
    for (int i = 0; i < size; i++)
        if (islower(msg[i]))
            msg[i] = toupper(msg[i]);
        else if (isupper(msg[i]))
            msg[i] = tolower(msg[i]);
}

void handler(int signum){

    if(signum == SIGUSR1){
        
        sleep(20); // --------------------------------------------------------------------------------------- >>>>>> COMMENT/UNCOMMENT to test multiple clients
        conv(shmaddr->buff);
        //printf("\nData found---------------------------------- = %s\n",shmaddr->buff);
        kill(shmaddr->clientpid, SIGUSR2);
                 
    }
}

void exitHandler(int signum){
     
    isExit = 1;  
}
    

int main()
{
    // Server code
    pid_t srvPID = getpid();
    int shmid;
    key_t key = 5000;

    
    // S E M A P H O R E S 
    int sem1 = create_sem(4000, 0);

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
    
    // prcess id of server
    shmaddr->serverpid = srvPID;

    UPSemapohre = up(sem1);

    // declaring the signal used by the client to inform the server that the client has write a msg 
    // SIGUSR1 >>> Server
    signal(SIGUSR1, handler);
    signal(SIGINT,exitHandler);

    while(isExit ==0){}

    // detach server
    shmdt((void*)shmaddr);
    // clear resources
    destroy_sem(sem1);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);

    return 0;
}
