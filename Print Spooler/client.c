/*
Name:ECSE427- MiniAssignment 2 
Author:Chuan Qin
ID:260562917
Date:2016-03-12.
*/


#include "common.h"

int fd;
int size;
int ID;// client ID
int pages;// number of pages will be printed
int temp;// store the next empty position
int waitflag = 0;// when it's 1, there is no empty slot and the process needs to wait.
int sval;//store the value of semaphore
Shared* shared_mem;

int setup_shared_memory(){//open the shared memory. The key MY_SHM is defined in commom.h file
    fd = shm_open(MY_SHM, O_RDWR, 0666);
    if(fd == -1){
        printf("shm_open() failed\n");
        exit(1);
    }
}

int attach_shared_memory(){//attach the shared memory.
    shared_mem = (Shared*) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(1);
    }
    return 0;
}

int unattach_shared_memory(){
    int i = munmap(shared_mem, sizeof(Shared));
    if(i == -1){
        printf("munmap() failed\n");
        exit(1);
    }
    return 0;
}

void get_job_params(int argc, char *argv[]){//get the job's parameters. argv[1] is the client ID and argv[2] is the number of pages to print.
    pages = atoi(argv[2]);
    ID = atoi(argv[1]);
}

void put_a_job(){//put a job to shared memory
    sem_getvalue(&(shared_mem->empty),&sval);//get the value of sem empty
    if(sval <= 0){//if it is less than 1 then there is no empty slot.
        waitflag = 1;//set the flag to 1
        printf("Client %d has %d pages to print, buffer full, sleeps\n",ID,pages);
    }
    sem_wait(&(shared_mem->empty));// block the process if there is not empty slot
    sem_wait(&(shared_mem->mutex));//mutex set to 0 to let only one process proceed the critical section.
    //sem_getvalue(&(shared_mem->empty),&sval);
    temp = shared_mem->clientindex;//get the index of array to store the parameters.
    if(shared_mem->clientindex == shared_mem->size -1){// update the index of array.
        shared_mem->clientindex = 0;
    }
    else{
        shared_mem->clientindex = shared_mem->clientindex +1;
    }
    shared_mem->arr[temp] = ID;//store the two parameters to sharedmemory.
    shared_mem->arr[temp+size] = pages;//the first half ofthe array stores ID and the second stores the num of page
    if(waitflag == 0){//if it is 0 then it means that the process has not sleeped 
        printf("Client %d has %d pages to print, puts request in Buffer[%d]\n", ID,pages,temp);
    }
    else if(waitflag == 1){// if it is 0 then it means that the process has sleeped before and indicate user that it wakes up.
        printf("Client %d wakes up, puts request in Buffer[%d]\n", ID,temp);
    }
    sem_post(&(shared_mem->mutex));//exit the critical section and set the mutex seg back to 1
    sem_post(&(shared_mem->full)); // number of full/used slot increases by 1.
}

int main(int argc, char *argv[]) {
    if(argc >3){
        printf("too many arguments!\n");
        exit(-1);
    }
    else if(argc <3){
        printf("arguments required!\n");
        exit(-1);
    }
    setup_shared_memory();//open the shared memory
    attach_shared_memory();//attach the shared memory
    size = shared_mem->size;//get the number of the slots from shared memory
    get_job_params(argc,argv); // get the job parameters.
    put_a_job(); // put the job parameters to shared memory
    unattach_shared_memory();//before exit, unattach the shared memory.
    close(fd);
    return 0;
}
