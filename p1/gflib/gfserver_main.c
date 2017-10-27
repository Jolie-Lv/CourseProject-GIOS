#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

#include "gfserver.h"
#include "content.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -p [listen_port]    Listen port (Default: 8140)\n"                         \
"  -c [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                             \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"content",       required_argument,      NULL,           'c'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);


/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short port = 8140;
  char *content = "content.txt";
  gfserver_t *gfs;

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:t:hc:", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'p': // listen-port
        port = atoi(optarg);
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
  
  content_init(content);

  /*Initializing server*/
  gfs = gfserver_create();

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 64);
  gfserver_set_handler(gfs, handler_get);
  gfserver_set_handlerarg(gfs, NULL);

  /*Loops forever*/
  gfserver_serve(gfs);
}
