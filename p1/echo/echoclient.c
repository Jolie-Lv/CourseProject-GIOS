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

/* Be prepared accept a response of this length */
#define BUFSIZE 2000

#define USAGE                                                                      \
    "usage:\n"                                                                     \
    "  echoclient [options]\n"                                                     \
    "options:\n"                                                                   \
    "  -s                  Server (Default: localhost)\n"                          \
    "  -p                  Port (Default: 8140)\n"                                 \
    "  -m                  Message to send to server (Default: \"hello world.\"\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"message", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8140;
    char *message = "hello world.";

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:m:h", gLongOptions, NULL)) != -1)
    {
        switch (option_char)
        {
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'm': // server
            message = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == message)
    {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535))
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    if (NULL == hostname)
    {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    int socket_desc;
    struct hostent *he;
    struct sockaddr_in server;
    char server_reply[16];

    //Create a socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1)
    {
	printf("Failed to create the socket!\n");
	exit(1);
    }

    if((he = gethostbyname(hostname)) == NULL)
    {
	printf("There is an error to get hostname!\n");
	exit(1);
    }
    memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
    server.sin_family = AF_INET;
    server.sin_port = htons(portno);

    //Connect to server
    if(connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
	printf("There is a connection error!\n");
	exit(1);
    }

    //send the message
    if(send(socket_desc, message, strlen(message), 0) < 0)
    {
	printf("THere is an error in sending the message!\n");
	exit(1);
    }
    //puts(message);
    //receive the echo from server
    if(recv(socket_desc, server_reply, 16, 0) < 0)
    {
	printf("There is an error in receiving the echo!\n");
    }
    printf("%s", server_reply);
    
    return 0;





}
