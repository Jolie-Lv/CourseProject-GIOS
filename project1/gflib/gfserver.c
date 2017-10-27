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

#include "gfserver.h"

/* 
 * Modify this file to implement the interface specified in
 * gfserver.h.
 */

struct gfcontext_t{
    int socket;
};

struct gfserver_t{
    unsigned short port;
    int max_npending;
    ssize_t (*handler)(gfcontext_t *, char *, void *);
    void* handlerarg;
    gfcontext_t *gfcontext;

};

void gfs_abort(gfcontext_t *ctx){
    close(ctx->socket);
    free(ctx);
}

gfserver_t* gfserver_create(){
    gfserver_t *gfs = (gfserver_t *)calloc(1, sizeof(gfserver_t));
    return gfs;
}

ssize_t gfs_send(gfcontext_t *ctx, void *data, size_t len){
    ssize_t writeBytes;
    writeBytes= write(ctx->socket, data, len);
    if(writeBytes < 0)
        return -1;

    return writeBytes;


}

ssize_t gfs_sendheader(gfcontext_t *ctx, gfstatus_t status, size_t file_len){
    char header[128];
    memset(header, '\0', sizeof(char) * 128);

    if(status == GF_OK){
        sprintf(header, "GETFILE OK %zu \r\n\r\n",  file_len );
    }else if(status == GF_FILE_NOT_FOUND){
        strcpy(header, "GETFILE FILE_NOT_FOUND \r\n\r\n");
    }else {
        strcpy(header, "GETFILE ERROR \r\n\r\n");
    }

    ssize_t writeBytes;
    writeBytes = write(ctx->socket, header, strlen(header));
    if(writeBytes < 0)
        return -1;

    return writeBytes;
}

void gfserver_serve(gfserver_t *gfs){
    int socket_desc, new_socket;
    struct sockaddr_in server, client;
    ssize_t bytesRd, headRd;
    int buffer_size = 1024;
    char buffer[buffer_size];
    char header[buffer_size];
    memset(buffer, '\0', sizeof(char) * buffer_size);


    //create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_desc == -1)
    {
        printf("Failed to create the socket!\n");
        exit(1);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(gfs->port);

    //bind
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0)
    {
        printf("setsockopt(SO_REUSEADDR) failed\n");
        exit(1);
    }
    if(bind(socket_desc, (struct sockaddr *)&(server), sizeof(server)) < 0)
    {
        printf("bind failed!\n");
        exit(1);
    }

    //listen
    listen(socket_desc,  gfs->max_npending);

    //accept incoming connections
    int c = sizeof(struct sockaddr_in);
    while((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)))
    {
        memset(header, '\0', sizeof(char) * buffer_size);
        headRd = 0;

        while(1)
        {
            bytesRd = read(new_socket, buffer, (size_t)buffer_size);
            if (bytesRd < 0) {
                printf("Read header error!\n");
                exit(1);
            }

            if (bytesRd == 0) {
                write(new_socket, "GETFILE ERROR \r\n\r\n", strlen("GETFILE ERROR \r\n\r\n"));
                break;
            }

            memcpy(header + headRd, buffer, (size_t)bytesRd);

            //search for \r\n\r\n
            char *p_rnrn;
            p_rnrn = strstr(header, "\r\n\r\n");
            if(p_rnrn != NULL){

                //split header by space
                char *ptok;
                ptok = strtok(header, " \r");
                char *scheme = ptok;
                ptok = strtok(NULL, " \r");
                char *method = ptok;
                ptok = strtok(NULL, " \r");
                char *filepath = ptok;


                gfcontext_t *gfcontext = (gfcontext_t *)calloc(1, sizeof(gfcontext_t));
                gfs->gfcontext = gfcontext;
                gfcontext->socket = new_socket;

                if(scheme == NULL || method == NULL || filepath == NULL) {
                    write(new_socket, "GETFILE FILE_NOT_FOUND \r\n\r\n", strlen("GETFILE FILE_NOT_FOUND \r\n\r\n"));
                    break;
                }else if(strcmp(scheme, "GETFILE") != 0 || strcmp(method, "GET") != 0 || strncmp(filepath, "/", 1) != 0){
                    write(new_socket, "GETFILE FILE_NOT_FOUND \r\n\r\n", strlen("GETFILE FILE_NOT_FOUND \r\n\r\n"));
                    break;
                }else{
                    gfs->handler(gfs->gfcontext, filepath, gfs->handlerarg);
                    break;
                }
            }else if(headRd >= buffer_size){
                write(new_socket, "GETFILE FILE_NOT_FOUND \r\n\r\n", strlen("GETFILE FILE_NOT_FOUND \r\n\r\n"));
                break;
            }
        }
        close(new_socket);
        
    }
}

void gfserver_set_handlerarg(gfserver_t *gfs, void* arg){
    gfs->handlerarg = arg;
}

void gfserver_set_handler(gfserver_t *gfs, ssize_t (*handler)(gfcontext_t *, char *, void*)){
    gfs->handler = handler;
}

void gfserver_set_maxpending(gfserver_t *gfs, int max_npending){
    gfs->max_npending = max_npending;
}

void gfserver_set_port(gfserver_t *gfs, unsigned short port){
    gfs->port = port;
}


