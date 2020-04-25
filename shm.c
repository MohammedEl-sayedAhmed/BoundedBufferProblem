#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>

void writer(int shmid)
{
    void *shmaddr = shmat(shmid, (void *)0, 0);
    if(shmaddr == -1)
    {
        perror("Error in attach in writer");
        exit(-1);
    }
    else
    {
        printf("\nShared memory attached at address %x\n", shmaddr);
        strcpy((char*) shmaddr, "Initial string!");
    }
    while(1)
    {
        scanf("%s", (char*) shmaddr);
        if(!strcmp((char*) shmaddr, "quit"))
            break;
    }
    shmdt(shmaddr);
}

void reader(int shmid)
{
    void *shmaddr = shmat(shmid, (void *)0, 0);
    if(shmaddr == -1)
    {
        perror("Error in attach in reader");
        exit(-1);
    }
    while(strcmp((char*) shmaddr, "quit"))
    {
        sleep(5);
        printf("\nData found = %s\n", (char*) shmaddr);
    }

    printf("\nAbout to destroy the shared memory area !\n");
    shmctl(shmid, IPC_RMID, (struct shmid_ds*)0);
}

int main()
{
    int shmid, pid;

    shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT|0644);

    if(shmid == -1)
    {
        perror("Error in create");
        exit(-1);
    }
    else
        printf("\nShared memory ID = %d\n", shmid);

    pid = fork();

    if(pid == 0)
        writer(shmid);
    else if(pid != -1)
        reader(shmid);
    else
    {
        perror("Error in fork");
        exit(-1);
    }
    return 0;
}
