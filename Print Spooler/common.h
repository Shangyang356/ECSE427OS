#ifndef _INCLUDE_COMMON_H_
#define _INCLUDE_COMMON_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
// from `man shm_open`
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <semaphore.h>

#define MY_SHM "/QC"

typedef struct {
    sem_t empty;
    sem_t full;
    sem_t mutex;
    int clientindex;
    int printerindex;
    int size;
    int arr[0];
} Shared;

#endif //_INCLUDE_COMMON_H_

