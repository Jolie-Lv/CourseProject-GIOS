#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>
#include <pthread.h>

#include "gfclient.h"
#include "workload.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n [num_requests]   Requests download per thread (Default: 2)\n"           \
"  -p [server_port]    Server port (Default: 8140)\n"                         \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -t [nthreads]       Number of threads (Default 2)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"help",          no_argument,            NULL,           'h'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {NULL,            0,                      NULL,             0}
};


static void Usage() {
	fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}


void *thread_function(void *arg){
    gfcrequest_t *gfr = (gfcrequest_t *)arg;
    intptr_t returncode = gfc_perform(gfr);
    return (void *)returncode;

}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
  char *server = "localhost";
  unsigned short port = 8140;
  char *workload_path = "workload.txt";

  int i = 0;
  int option_char = 0;
  int nrequests = 2;
  int nthreads = 2;
  int returncode = 0;
  char *req_path = NULL;
  char local_path[512];

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "hn:p:s:t:w:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'h': // help
        Usage();
        exit(0);
        break;                      
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 'p': // port
        port = atoi(optarg);
        break;
      case 's': // server
        server = optarg;
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'w': // workload-path
        workload_path = optarg;
        break;
      default:
        Usage();
        exit(1);
    }
  }

  if( EXIT_SUCCESS != workload_init(workload_path)){
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }

  gfc_global_init();


    pthread_t client_t[nthreads];
    gfcrequest_t *gfr[nthreads];
    FILE *file[nthreads];
    

    int j = 0;

    for (i = 0; i < nrequests; i++) {

        for (j = 0; j < nthreads; j++) {
            req_path = workload_get_path();

            if (strlen(req_path) > 256) {
                fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
                exit(EXIT_FAILURE);
            }

            localPath(req_path, local_path);
            file[j] = openFile(local_path);

            gfr[j] = gfc_create();;
            gfc_set_server(gfr[j], server);
            gfc_set_path(gfr[j], req_path);
            gfc_set_port(gfr[j], port);
            gfc_set_writefunc(gfr[j], writecb);
            gfc_set_writearg(gfr[j], file[j]);

            fprintf(stdout, "Requesting %s%s\n", server, req_path);

            pthread_create(&client_t[j], NULL, thread_function, gfr[j]);


        }

        for (j = 0; j < nthreads; j++) {
            pthread_join(client_t[j], (void **) &returncode);

            if (0 > returncode) {
                fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
                fclose(file[j]);
                if (0 > unlink(local_path))
                    fprintf(stderr, "unlink failed on %s\n", local_path);
            } else {
                fclose(file[j]);
            }

            if (gfc_get_status(gfr[j]) != GF_OK) {
                if (0 > unlink(local_path))
                    fprintf(stderr, "unlink failed on %s\n", local_path);
            }

            fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr[j])));
            fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr[j]), gfc_get_filelen(gfr[j]));

            gfc_cleanup(gfr[j]);

        }
    }

  gfc_global_cleanup();

  return 0;
}  
