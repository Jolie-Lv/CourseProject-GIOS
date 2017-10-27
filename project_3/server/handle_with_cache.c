#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/shm.h>

#include "gfserver.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

struct shared_info {
    int nthread;
    int nseg;
    size_t segsize;
    char **ptr_shm;
    int *ptr_status;
    sem_t **sem_reader;
    sem_t **sem_writer;
};

typedef struct task{
    gfcontext_t *ctx;
    char* path;
    struct shared_info *infopass;

}task;

int workThreads;
pthread_mutex_t mutex_th;
pthread_cond_t capable_th;
pthread_mutex_t mutex_seg;
pthread_cond_t capable_seg;


//Replace with an implementation of handle_with_cache and any other
//functions you may need.

void* thread_function(void* arg){
    task *newTask = (task *)arg;
    struct shared_info *infoToPass = newTask->infopass;

    int seg_available, count, filelen;
    intptr_t bytes_send;
    size_t send_len;

    //check if there are still workers available
    pthread_mutex_lock(&mutex_th);
    while(workThreads >= infoToPass->nthread)
        pthread_cond_wait(&capable_th, &mutex_th);
    workThreads++;
    pthread_mutex_unlock(&mutex_th);

    //compose message to send
    char message[1024];
    sprintf(message, "%s\r\n\r\n", newTask->path);
    //printf("the message is: %s", message);

    //create UNIX domain socket
    int socket_desc;
    struct sockaddr_un addr;

    if((socket_desc = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        perror("AF_UNIX Socket error");
        exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "socket", sizeof(addr.sun_path) - 1);

    //connect to server (simplecached.c)
    if(connect(socket_desc, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("AF_UNIX Connect error");
        exit(-1);
    }

    //send message to server
    if(send(socket_desc, message, strlen(message), 0) < 0){
        perror("AF_UNIX Send error");
        exit(-1);
    }

    //receive reply from server
    char cache_reply[128];
    if(recv(socket_desc, cache_reply, sizeof(cache_reply), 0) < 0){
        perror("AF_UNIX Recv error");
        exit(-1);
    }

    //read status code and file length from server reply
    char status_code[128];
    count = sscanf(cache_reply, "%s %d\r\n\r\n", status_code, &filelen);
    //printf("status code: %s, filelen: %d\n", status_code, filelen);

    if(count != 2){
        printf("Sscanf error, count: %d\n", count);
        exit(-1);
    }

    if(strcmp(status_code, "FILE_NOT_FOUND") == 0){
        //printf("Proxy send message GF_FILE_NOT_FOUND\n");

        //terminate a thread, and signal others
        pthread_mutex_lock(&mutex_th);
        workThreads--;
        pthread_cond_signal(&capable_th);
        pthread_mutex_unlock(&mutex_th);

        return (void *)gfs_sendheader(newTask->ctx, GF_FILE_NOT_FOUND, 0);;

    }else{
        //check if there are still segments available
        pthread_mutex_lock(&mutex_seg);
        while(infoToPass->ptr_status[0] < 1)
            pthread_cond_wait(&capable_seg, &mutex_seg);
        infoToPass->ptr_status[0]--;
        for(seg_available = 0; seg_available < infoToPass->nseg; seg_available++){
            if(infoToPass->ptr_status[seg_available + 1] == 1){
                infoToPass->ptr_status[seg_available + 1] = 0;
                break;
            }
        }
        pthread_mutex_unlock(&mutex_seg);

        //printf("Proxy send message GF_OK and filelen: %d\n", filelen);
        gfs_sendheader(newTask->ctx, GF_OK, filelen);

        //send segment info to simplecached
        sprintf(message, "%d %zu\r\n\r\n", seg_available, infoToPass->segsize);
        //printf("the available segment info: %s\n", message);
        if(send(socket_desc, message, strlen(message), 0) < 0){
            perror("AF_UNIX Send error");
            exit(-1);
        }

        bytes_send = 0;
        while(bytes_send < filelen){
            //reader semaphore
            sem_wait(infoToPass->sem_reader[seg_available]);
            send_len = gfs_send(newTask->ctx, infoToPass->ptr_shm[seg_available] + 4, *(int *)(infoToPass->ptr_shm[seg_available]));

            //signal writer semaphore
            sem_post(infoToPass->sem_writer[seg_available]);

            bytes_send += send_len;

        }
        //printf("send_len: %zu, bytes_send: %zu\n", send_len, bytes_send);

        //release segments for others to use
        pthread_mutex_lock(&mutex_seg);
        infoToPass->ptr_status[0]++;
        infoToPass->ptr_status[seg_available + 1] = 1;
        pthread_cond_signal(&capable_seg);
        pthread_mutex_unlock(&mutex_seg);

        //terminate a thread
        pthread_mutex_lock(&mutex_th);
        workThreads--;
        pthread_cond_signal(&capable_th);
        pthread_mutex_unlock(&mutex_th);

        return (void *)bytes_send;
    }

}

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg) {
    task *newTask = (task *) malloc(sizeof(task));
    newTask->ctx = ctx;
    newTask->path = path;
    newTask->infopass = (struct shared_info *) arg;

    pthread_t newThread;
    pthread_create(&newThread, NULL, thread_function, newTask);

    return 1;

}


