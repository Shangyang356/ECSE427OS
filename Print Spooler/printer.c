/*
Name:ECSE427- MiniAssignment 2 
Author:Chuan Qin
ID:260562917
Date:2016-03-12.
*/

#include "common.h"
#include <unistd.h>

int fd;
int jobpages;
int jobID;
int position;
int temp;
int MY_LEN;
int sval;
Shared* shared_mem;


int setup_shared_memory(){//create a shared memory
    fd = shm_open(MY_SHM, O_CREAT | O_RDWR, 0666);
    if(fd == -1){
        printf("shm_open() failed\n");
        exit(1);
    }
    ftruncate(fd, sizeof(Shared) + 2*MY_LEN*sizeof(int));
}

int unlink_shared_memory(){// unlink the shared memory.
    fd = shm_unlink(MY_SHM);
    if(fd == -1){
        printf("shm_unlink() failed\n");
        exit(1);
    }
}

int attach_shared_memory(){//attach the created shared memory
    shared_mem = (Shared*)  mmap(NULL, sizeof(Shared) + 2*MY_LEN*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared_mem == MAP_FAILED){
        printf("mmap() failed\n");
        exit(1);
    }
    return 0;
}

int unattach_shared_memory(){//unattach the shared memory
    int i = munmap(shared_mem, sizeof(Shared));
    if(i == -1){
        printf("munmap() failed\n");
        exit(1);
    }
    return 0;
}

int init_shared_memory() {//initialize the variables on the share memory
    shared_mem->clientindex = 0;//set two indices to 0 
    shared_mem->printerindex = 0;
    shared_mem->size = MY_LEN;//size equals to the number of slots
    sem_init(&(shared_mem->full), 1, 0);// 
    sem_init(&(shared_mem->empty), 1, MY_LEN);
    sem_init(&(shared_mem->mutex), 1, 1);
    int i;
    for (i=0; i < 2*MY_LEN; i++) {//
        shared_mem->arr[i] = -1;
    }
}

void take_a_job(){
    //sem_getvalue(&(shared_mem->mutex),&sval);
    //while(sval == 0){
        //sem_getvalue(&(shared_mem->mutex),&sval);
    //}
    sem_getvalue(&(shared_mem->full),&sval);//get the value of sem full.
    if(sval <= 0){ // if its value is 0 then there is no job to print.
        printf("-------------------------------------\n");
        printf("No request in buffer, Printer sleeps\n");
        printf("-------------------------------------\n");
    }
    sem_wait(&(shared_mem->full));
    sem_wait(&(shared_mem->mutex));
    position = shared_mem->printerindex;//get the index of array 
    jobID = shared_mem->arr[position];//read the parameters that will be printed.    
    jobpages = shared_mem->arr[position+MY_LEN];
    sem_post(&(shared_mem->mutex));
    //sem_post(&(shared_mem->empty));
}

void print_a_msg(){//print start message
    printf("Printer starts printing Client %d, %d pages from Buffer[%d]\n", jobID,jobpages,position);
}

void go_sleep(){
    sleep(jobpages);//sleep 
    if(shared_mem->printerindex == MY_LEN -1){//update the index of the array for next print task.
        shared_mem->printerindex = 0;
    }
    else{
        shared_mem->printerindex = shared_mem->printerindex + 1;
    }
    sem_post(&(shared_mem->empty));//once finished, run signal(empty);
    printf("Printer finishes printing Client%d, %d pages from Buffer[%d]\n",jobID,jobpages,position);
}

void detectctrl(int sig){//detect whether user has pressed ctrl + C. 
    unattach_shared_memory();
    unlink_shared_memory();
    close(fd);
    printf("exit the printer server and release the shared memory!\n");
    exit(0);

} 
int main(int argc, char *argv[]) {
    if(argc > 2){//check the number of command line parameter
        printf("too many arguments!\n");
        exit(-1);
    }
    else if(argc < 2){
        printf("arguments required!\n");
        exit(-1);
    }
    MY_LEN = atoi(argv[1]);//the second command line parameter is the num of slots
    if(MY_LEN <=0){
        printf("Invalid slot number!\n");
        exit(-1);
    }
    setup_shared_memory(); //set up,attach and initialize the shared memory.
    attach_shared_memory();
    init_shared_memory();
    while (1) {
        signal(SIGINT,detectctrl);
        take_a_job();
        print_a_msg();
        go_sleep();
        
    }
    return 0;
}
