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

#define BUFSIZE 2000

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferclient [options]\n"                           \
    "options:\n"                                             \
    "  -s                  Server (Default: localhost)\n"    \
    "  -p                  Port (Default: 8140)\n"           \
    "  -o                  Output file (Default gios.txt)\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"output", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 8140;
    char *filename = "gios.txt";

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:h", gLongOptions, NULL)) != -1)
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
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
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

    if (NULL == hostname)
    {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    int socket_desc, bytesRd;
    struct hostent *he;
    struct sockaddr_in server;
    char buffer[BUFSIZE];
    FILE *fp;
    memset(buffer, '\0', sizeof(buffer));

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
    
    fp = fopen(filename, "ab");
    if(fp == NULL)
    {
	printf("Error opening the file");
	exit(1);
    }
    while((bytesRd = read(socket_desc, buffer, BUFSIZE)))
    {
	fwrite(buffer, 1, bytesRd, fp);
        //memset(buffer, '\0', sizeof(buffer));
    }

    if(bytesRd < 0)
    {
	printf("Read error!\n");
        exit(1);
    }
    //printf("File transfer is completed! \n");
    close(socket_desc);
    fclose(fp);
    
    return 0;
}
