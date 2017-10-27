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
/* 
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
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

#define BUFSIZE 4000

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: cs8803.txt)\n" \
    "  -h                  Show this help message\n"         \
    "  -p                  Port (Default: 8140)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 8140;             /* port to listen on */
    char *filename = "cs8803.txt"; /* file to transfer */

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        case 'f': // listen-port
            filename = optarg;
            break;
        }
    }

    if (NULL == filename)
    {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    int socket_desc, new_socket;
    struct sockaddr_in server, client;
    char buffer[BUFSIZE];    
    FILE *fp;
    memset(buffer, '\0', sizeof(buffer));
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
    listen(socket_desc,  5);

    //accept incoming connections
    int c = sizeof(struct sockaddr_in);
    while((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
    {	
	fp = fopen(filename , "rb");
	if(fp == NULL)
        {
            printf("Error in open the file! \n");
            exit(1);
	}

	while(1)
	{
	    //memset(buffer, '\0', sizeof(buffer));
	    int nread = fread(buffer, 1, BUFSIZE, fp);
	    if(nread > 0)
            {
                //printf("Sending \n");
                write(new_socket, buffer, nread);
            }
            if (nread < BUFSIZE)
            {
                if (feof(fp))
		    printf("End of file\n");
		if (ferror(fp))
                    printf("Error reading\n");
                break;
            }
	    
	}
	
        fclose(fp);
	close(new_socket);
    }


    if(new_socket < 0)
    {
	printf("Failed to accept!\n");
	exit(1);
    }

    close(new_socket);
    return 0;
}
