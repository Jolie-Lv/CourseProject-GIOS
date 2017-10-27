#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"

#define BUFFER_SIZE 4000

typedef struct task{
    gfcontext_t *ctx;
    char* path;
    intptr_t maxThreads;

}task;

int workThreads;
pthread_mutex_t mutex;
pthread_cond_t capable;

void * thread_function(void * arg){
    task * newTask = (task *)arg;

    int fildes;
    intptr_t bytes_transferred;
    size_t file_len;
    ssize_t read_len, write_len;
    char buffer[BUFFER_SIZE];

    //check if there are still workers available
    pthread_mutex_lock(&mutex);
    while(workThreads >= newTask->maxThreads)
        pthread_cond_wait(&capable, &mutex);
    workThreads++;
    pthread_mutex_unlock(&mutex);

    //start to work
    if( 0 > (fildes = content_get(newTask->path))){

        pthread_mutex_lock(&mutex);
        workThreads--;
        pthread_cond_signal(&capable);
        pthread_mutex_unlock(&mutex);
        //free(newTask);
        return (void *)gfs_sendheader(newTask->ctx, GF_FILE_NOT_FOUND, 0);
    }


    // Determine the file size
    file_len = lseek(fildes, 0, SEEK_END);

    gfs_sendheader(newTask->ctx, GF_OK, file_len);

    // Send the file in chunks
    bytes_transferred = 0;
    while(bytes_transferred < file_len){

        read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
        if (read_len <= 0){
            fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu\n", read_len, bytes_transferred, file_len );

            bytes_transferred = -1;
            break;
        }
        write_len = gfs_send(newTask->ctx, buffer, read_len);
        if (write_len != read_len){
            fprintf(stderr, "handle_with_file write error, %zd != %zd\n", write_len, read_len);

            bytes_transferred = -1;
            break;
        }


        bytes_transferred += write_len;
    }

    pthread_mutex_lock(&mutex);
    workThreads--;
    pthread_cond_signal(&capable);
    pthread_mutex_unlock(&mutex);

    return (void *)bytes_transferred;


}


ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg) {
    task* newTask = (task*)malloc(sizeof(task));
    newTask->ctx = ctx;
    newTask->path = path;
    newTask->maxThreads = (intptr_t)arg;

    pthread_t newThread;
    pthread_create(&newThread, NULL, thread_function, newTask);

    return 1;

}




