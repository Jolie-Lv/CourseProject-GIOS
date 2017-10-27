
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -t [nthreads]       Number of threads (Default: 2)\n"                      \
"  -p [listen_port]    Listen port (Default: 8140)\n"                         \
"  -c [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                             \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"content",       required_argument,      NULL,           'c'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);
extern void * thread_function(void* arg);
extern int workThreads;
extern pthread_mutex_t mutex;
extern pthread_cond_t capable;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    exit(signo);
  }
}



/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short port = 8140;
  char *content = "content.txt";
  gfserver_t *gfs = NULL;
  intptr_t nthreads = 2;

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:t:hc:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;       
      case 'c': // file-path
        content = optarg;
        break;                                          
    }
  }

  /* not useful, but it ensures the initial code builds without warnings */
  if (nthreads < 1) {
    nthreads = 1;
  }
  
  content_init(content);


  /*Initializing server*/
  gfs = gfserver_create();
  workThreads = 0;

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 64);
  gfserver_set_handler(gfs, handler_get);
  gfserver_set_handlerarg(gfs, (void *)nthreads);
  gfserver_serve(gfs);




}
