#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>

#if 0
/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

#define BUFSIZE 2000

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -p                  Port (Default: 8140)\n"                                \
"  -m                  Maximum pending connections (default: 8)\n"            \
"  -h                  Show this help message\n"                              \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"maxnpending",   required_argument,      NULL,           'm'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


int main(int argc, char **argv) {
  int option_char;
  int portno = 8140; /* port to listen on */
  int maxnpending = 8;
  
  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:m:h", gLongOptions, NULL)) != -1) {
   switch (option_char) {
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'p': // listen-port
        portno = atoi(optarg);
        break;                                        
      case 'm': // server
        maxnpending = atoi(optarg);
        break; 
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;
    }
  }

    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    int socket_desc, new_socket;
    struct sockaddr_in server, client;
    char buffer[16];

    //create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    
    if(socket_desc == -1)
    {
	printf("Failed to create the socket!\n");
	exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portno);

    //bind
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    {
        printf("setsockopt(SO_REUSEADDR) failed\n");
	exit(1);
    }
    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) 
    {
	printf("bind failed!\n");
	exit(1);
    }

    //listen
    listen(socket_desc,  maxnpending);

    //accept incoming connections
    int c = sizeof(struct sockaddr_in);
    while((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
    {	
	memset(buffer, 0, sizeof(buffer));
	if(recv(new_socket, buffer, 16, 0) < 0)
    	{
	    printf("There is an error in receiving the message!\n");
	    exit(1);
    	}
	//reply to the client
	//puts(buffer);
	write(new_socket, buffer, 16);
        
	
    }

    if(new_socket < 0)
    {
	printf("Failed to accept!\n");
	exit(1);
    }

    return 0;
 
}
