#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>

#include "gfserver.h"
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/mman.h>

/* note that the -n and -z parameters are NOT used for Part 1 */
/* they are only used for Part 2 */                         
#define USAGE                                                                         \
"usage:\n"                                                                            \
"  webproxy [options]\n"                                                              \
"options:\n"                                                                          \
"  -n [segment_count]  Number of segments to use (Default: 2)\n"                      \
"  -p [listen_port]    Listen port (Default: 8140)\n"                                 \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1024)\n"              \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)\n"     \
"  -z [segment_size]   The segment size (in bytes, Default: 256).\n"                  \
"  -h                  Show this help message\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"segment-count", required_argument,      NULL,           'n'},
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},
  {"segment-size",  required_argument,      NULL,           'z'},         
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,            0}
};

struct shared_info {
    int nthread;
    int nseg;
    size_t segsize;
    char **ptr_shm;
    int *ptr_status;
    sem_t **sem_reader;
    sem_t **sem_writer;

};


//extern ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg);
extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;

extern int workThreads;
extern pthread_mutex_t mutex_th;
extern pthread_cond_t capable_th;
extern pthread_mutex_t mutex_seg;
extern pthread_cond_t capable_seg;


int nseg;

void shm_clean(){
    char shm_path[128];
    for(int i = 0; i < nseg; i++){
        sprintf(shm_path, "/shm_seg_%d", i);
        shm_unlink(shm_path);
        sprintf(shm_path, "/sem_reader%d", i);
        sem_unlink(shm_path);
        sprintf(shm_path, "/sem_writer%d", i);
        sem_unlink(shm_path);
    }
}

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    shm_clean();
    gfserver_stop(&gfs);
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i;
  int option_char = 0;
  unsigned short port = 8140;
  unsigned short nworkerthreads = 1;
  unsigned int nsegments = 2;
  size_t segsize = 256;
  char *server = "s3.amazonaws.com/content.udacity-data.com";

  /* disable buffering on stdout so it prints immediately */
  setbuf(stdout, NULL);

  if (signal(SIGINT, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(SERVER_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR) {
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(SERVER_FAILURE);
  }

  /* Parse and set command line arguments */
  while ((option_char = getopt_long(argc, argv, "n:p:s:t:z:h", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'n': // segment count
        nsegments = atoi(optarg);
        break;   
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 's': // file-path
        server = optarg;
        break;                                          
      case 't': // thread-count
        nworkerthreads = atoi(optarg);
        break;
      case 'z': // segment size
        segsize = atoi(optarg);
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(__LINE__);
    }
  }

  if (!server) {
    fprintf(stderr, "Invalid (null) server name\n");
    exit(__LINE__);
  }

  if (segsize < 128) {
    fprintf(stderr, "Invalid segment size\n");
    exit(__LINE__);
  }

  if (nsegments < 1) {
    fprintf(stderr, "Must have a positive number of segments\n");
    exit(__LINE__);
  }

 if (port < 1024) {
    fprintf(stderr, "Invalid port number\n");
    exit(__LINE__);
  }

  if ((nworkerthreads < 1) || (nworkerthreads > 1024)) {
    fprintf(stderr, "Invalid number of worker threads\n");
    exit(__LINE__);
  }


  /* This is where you initialize your shared memory */
    nseg = nsegments;
    struct shared_info *infoToPass;
    char *seg_ptr[nsegments];
    int seg_status[nsegments + 1];
    seg_status[0] = nsegments;
    sem_t *sem_reader[nsegments];
    sem_t *sem_writer[nsegments];
    infoToPass = (struct shared_info *)malloc(sizeof(struct shared_info));
    infoToPass->nthread = nworkerthreads;
    infoToPass->nseg = nsegments;
    infoToPass->segsize = segsize;



    //create shared memory and shared semaphores

    int shmid;
    char shm_path[128];

    for(i = 0; i < nsegments; i++){
        //shared memory
        sprintf(shm_path, "/shm_seg_%d", i);

        if((shmid = shm_open(shm_path, O_CREAT|O_RDWR, 0666)) < 0){
            printf("shm_open error");
            exit(-1);
        }

        ftruncate(shmid, segsize);
        //mmap shared memory to char pointer
        seg_ptr[i] = mmap(0, segsize, O_RDWR, MAP_SHARED, shmid, 0);
        if(seg_ptr[i] == MAP_FAILED){
            printf("mmap error %d", i);
            exit(-1);
        }

        //initialize segments status
        seg_status[i + 1] = 1;

        //shared semaphore
        sprintf(shm_path, "/sem_reader_%d", i);
        sem_reader[i] = sem_open(shm_path, O_CREAT|O_EXCL, 0666, 0);
        if(sem_reader[i] == SEM_FAILED){
            printf("sem_reader open error");
            exit(-1);
        }

        sprintf(shm_path, "/sem_writer_%d", i);
        sem_writer[i] = sem_open(shm_path, O_CREAT|O_EXCL, 0666, 1);
        if(sem_writer[i] == SEM_FAILED){
            printf("sem_writer open error");
            exit(-1);
        }


    }

    infoToPass->ptr_shm = seg_ptr;
    infoToPass->ptr_status = seg_status;
    infoToPass->sem_reader = sem_reader;
    infoToPass->sem_writer = sem_writer;


  /* This is where you initialize the server struct */
  gfserver_init(&gfs, nworkerthreads);
  workThreads = 0;

  /* This is where you set the options for the server */
  gfserver_setopt(&gfs, GFS_PORT, port);
  gfserver_setopt(&gfs, GFS_MAXNPENDING, 12);
  gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
  for(i = 0; i < nworkerthreads; i++) {
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, infoToPass);
  }
  
  /* This is where you invoke the framework to run the server */
  /* Note that it loops forever */
  gfserver_serve(&gfs);





}
