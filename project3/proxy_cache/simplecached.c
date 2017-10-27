#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include "shm_channel.h"
#include "simplecache.h"
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 1024

struct cacheTask{
    int socket_new;
    char *buffer;
};

int maxTheads;
int currentWorkers;
int socket_serv;
pthread_mutex_t mutex_cache;
pthread_cond_t capable_cache;



void* thread_function(void * arg){

    struct cacheTask *newTask = (struct cacheTask *)arg;

    int count, nseg, fildes, filelen;
    size_t segsize;
    char cache_key[1024];
    char cache_reply[64];
    char seg_info[256];

    //check if there are still workers available
    pthread_mutex_lock(&mutex_cache);
    while(currentWorkers >= maxTheads)
        pthread_cond_wait(&capable_cache, &mutex_cache);
    currentWorkers++;
    pthread_mutex_unlock(&mutex_cache);

    count = sscanf(newTask->buffer, "%s\r\n\r\n", cache_key);
    //printf("cache key is: %s\n", cache_key);
    if(count != 1){
        printf("Sscanf error, count: %d\n", count);
        exit(-1);
    }

    fildes = simplecache_get(cache_key);

    if(fildes == -1){
        strcpy(cache_reply, "FILE_NOT_FOUND 0\r\n\r\n");
        //printf("cache reply: %s\n", cache_reply);
        write(newTask->socket_new, cache_reply, strlen(cache_reply));
        close(newTask->socket_new);

        pthread_mutex_lock(&mutex_cache);
        currentWorkers--;
        pthread_cond_signal(&capable_cache);
        pthread_mutex_unlock(&mutex_cache);
        return (void *)0;

    }else{
        filelen = lseek(fildes, 0, SEEK_END);
        lseek(fildes, 0, SEEK_SET);

        sprintf(cache_reply, "OK %d\r\n\r\n", filelen);
        //printf("cache reply: %s\n", cache_reply);
        write(newTask->socket_new, cache_reply, strlen(cache_reply));

        if(read(newTask->socket_new, seg_info, sizeof(seg_info)) > 0) {
            count = sscanf(seg_info, "%d %zu\r\n\r\n", &nseg, &segsize);
            //printf("nsegment: %d, segsize: %zu\n", nseg, segsize);
            if (count != 2) {
                printf("Sscanf2 error, count: %d\n", count);
                exit(-1);
            }

            char *shared_buffer;
            char shm_path[128];
            int shmid;
            sem_t *sem_reader;
            sem_t *sem_writer;

            //link shared memory
            sprintf(shm_path, "/shm_seg_%d", nseg);
            if((shmid = shm_open(shm_path, O_RDWR, 0666)) < 0){
                printf("shm_open error");
                exit(-1);
            }

            ftruncate(shmid, segsize);

            shared_buffer = mmap(0, segsize, O_RDWR, MAP_SHARED, shmid, 0);
            if(shared_buffer == MAP_FAILED){
                printf("mmap error");
                exit(-1);
            }

            //link shared reader semaphore
            sprintf(shm_path, "/sem_reader_%d", nseg);
            sem_reader = sem_open(shm_path, O_RDWR);
            if(sem_reader == SEM_FAILED){
                printf("sem_reader open error");
                exit(-1);
            }

            //link shared writer semaphore
            sprintf(shm_path, "/sem_writer_%d", nseg);
            sem_writer = sem_open(shm_path, O_RDWR);
            if(sem_writer == SEM_FAILED){
                printf("sem_writer open error");
                exit(-1);
            }

            //write data into shared segment
            size_t bytes_read,  read_len;
            bytes_read = 0;

            while(bytes_read < filelen){

                //open writer semaphore
                sem_wait(sem_writer);
                read_len = read(fildes, shared_buffer + 4, segsize - 16);
                if (read_len < 0){
                    printf("simplecached read error");
                    exit(-1);
                }
                int *read_len_ptr = (int *)shared_buffer;
                *read_len_ptr = read_len;

                //signal reader semaphore
                sem_post(sem_reader);

                bytes_read += read_len;
            }
            //printf("read_len: %zu, bytes_send: %zu\n", read_len, bytes_read);
            close(newTask->socket_new);

            pthread_mutex_lock(&mutex_cache);
            currentWorkers--;
            pthread_cond_signal(&capable_cache);
            pthread_mutex_unlock(&mutex_cache);
            return (void *)1;
        }

        printf("read seg segsize error");
        exit(-1);
    }

}

static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1024)\n"      \
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {"nthreads",           required_argument,      NULL,           't'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	int nthreads = 1;
	char *cachedir = "locals.txt";
	char option_char;


    /* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "c:ht:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			default:
				Usage();
				exit(1);
		}
	}

	if ((nthreads>1024) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}

	/* Cache initialization */
	simplecache_init(cachedir);

	/* Add your cache code here */
    maxTheads = nthreads;
    currentWorkers = 0;

    //create UNIX domain socket
    int socket_new;
    struct sockaddr_un addr;

    if((socket_serv = socket(AF_UNIX, SOCK_STREAM, 0)) < 0){
        perror("Socket error");
        exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "socket", sizeof(addr.sun_path) - 1);
    unlink("socket");

    //bind
    if(bind(socket_serv, (struct sockaddr*)&addr, sizeof(addr)) < 0){
        perror("Bind error");
        exit(-1);
    }

    //listen
    if(listen(socket_serv, 10) < 0){
        perror("Listen error");
        exit(-1);
    }

    char buffer[MAX_CACHE_REQUEST_LEN];
    while(1){
        //accept new request
        if((socket_new = accept(socket_serv, NULL, NULL)) < 0){
            printf("error, accept socket_new");
            exit(-1);
        }

        if(read(socket_new, buffer, sizeof(buffer)) > 0) {
            struct cacheTask *newTask = (struct cacheTask *) malloc(sizeof(struct cacheTask));
            newTask->socket_new = socket_new;
            newTask->buffer = (char *)malloc(MAX_CACHE_REQUEST_LEN * sizeof(char));
            memcpy(newTask->buffer, buffer, strlen(buffer));

            //start a new thread
            pthread_t newThread;
            pthread_create(&newThread, NULL, thread_function, newTask);

        }



    }




	/* this code probably won't execute */
	return 0;
}
