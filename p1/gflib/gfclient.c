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

#include "gfclient.h"

/*struct for a getfile request*/
struct gfcrequest_t{

    char *server;
    unsigned short port;
    char *path;
    char *headerarg;
    void (*headerfunc) (void*, size_t, void*);
    FILE *writearg;
    void (*writefunc) (void*, size_t, void*);
    gfstatus_t status;
    size_t bytesreceived, filelen;

};

void gfc_cleanup(gfcrequest_t *gfr){

    if(gfr != NULL){
        free(gfr->server);
        free(gfr->path);
        free(gfr);
    }

}

gfcrequest_t *gfc_create(){

    gfcrequest_t *gfr = (gfcrequest_t *)calloc(1, sizeof(gfcrequest_t));
    gfr->server = (char *)calloc(1024, sizeof(char));
    gfr->path = (char *)calloc(1024, sizeof(char));
    gfr->filelen = 0;
    gfr->bytesreceived = 0;



    return gfr;

}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    return gfr->bytesreceived;
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
    return gfr->filelen;
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    return gfr->status;
}

void gfc_global_init(){

}

void gfc_global_cleanup(){

}

int gfc_perform(gfcrequest_t *gfr) {

    /* Socket Code Here */
    int socket_desc;
    ssize_t bytesRd;
    int headBytesRd = 0;
    int header_found = 0;
    size_t buffer_size = 1024;
    struct hostent *he;
    struct sockaddr_in server;


    //Create a socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_desc == -1) {
        printf("Failed to create the socket!\n");
        return -1;
    }

    if ((he = gethostbyname(gfr->server)) == NULL) {
        printf("There is an error to get hostname!\n");
        return -1;
    }
    memcpy(&server.sin_addr, he->h_addr_list[0], (size_t) he->h_length);
    server.sin_family = AF_INET;
    server.sin_port = htons(gfr->port);

    //Connect to server
    if (connect(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0) {
        printf("There is a connection error!\n");
        return -1;
    }

    char *request = (char *)calloc(1024, sizeof(char));

    //send the message
    sprintf(request, "GETFILE GET %s\r\n\r\n", gfr->path);
    if (send(socket_desc, request, strlen(request), 0) < 0) {
        printf("There is an error in sending the message!\n");
        gfr->status = GF_INVALID;
        free(request);
        return -1;
    }
    free(request);
    //receive data from server
    char *header = (char *)calloc(buffer_size, sizeof(char));
    char *buffer = (char *)calloc(buffer_size, sizeof(char));


    while (1) {


        bytesRd = read(socket_desc, buffer, buffer_size);

        if (bytesRd < 0) {
            printf("Read header error!\n");
            //gfr->status = GF_INVALID;
            //free(buffer);
            //free(header);
            //close(socket_desc);
            return -1;
        }

        else if (bytesRd == 0) {

            gfr->status = GF_INVALID;
            if(header_found == 1)
                gfr->status = GF_OK;

            free(buffer);
            free(header);
            close(socket_desc);
            return -1;
        }
        //receive header
        else if(header_found == 0){

            memcpy(header + headBytesRd, buffer, (size_t)bytesRd);
            headBytesRd += bytesRd;

            //if find the valid end \r\n\r\n
            char *end_tag = strstr(header, "\r\n\r\n");
            //if(strstr(header, "\r\n\r\n") != NULL)
            if(end_tag != NULL)
            {
                header_found = 1;
                int filelen = -1;
                int count = sscanf(header, "GETFILE OK %d\r\n\r\n", &filelen);

                if (count == 1 && filelen > 0){
                    gfr->status = GF_OK;
                    gfr->filelen = (size_t)filelen;

                    char temp[32] = {"\0"};
                    sprintf(temp, "GETFILE OK %d\r\n\r\n", filelen);
                    size_t extraBytes = headBytesRd - strlen(temp);
                    //size_t extraBytes = strlen(end_tag + 4);
                    if(extraBytes >= gfr->filelen) {
                        gfr->writefunc(header + strlen(temp), gfr->filelen, gfr->writearg);
                        //gfr->writefunc(buffer + bytesRd - extraBytes, gfr->filelen, gfr->writearg);
                        //gfr->writefunc(end_tag + 4, gfr->filelen, gfr->writearg);
                        gfr->bytesreceived = gfr->filelen;
                        free(buffer);
                        free(header);
                        close(socket_desc);
                        return 0;
                    }else if(extraBytes > 0){
                        gfr->writefunc(header + strlen(temp), extraBytes, gfr->writearg);
                        //gfr->writefunc(buffer + bytesRd - extraBytes, gfr->filelen, gfr->writearg);
                        //gfr->writefunc(end_tag + 4, gfr->filelen, gfr->writearg);
                        gfr->bytesreceived += extraBytes;
                    }
                }else if(strcmp(header, "GETFILE FILE_NOT_FOUND\r\n\r\n") == 0){
                    gfr->status = GF_FILE_NOT_FOUND;
                    free(buffer);
                    free(header);
                    close(socket_desc);
                    return 0;
                }else if(strcmp(header, "GETFILE ERROR\r\n\r\n") == 0){
                    gfr->status = GF_ERROR;
                    free(buffer);
                    free(header);
                    close(socket_desc);
                    return 0;
                }else{
                    gfr->status = GF_INVALID;
                    free(buffer);
                    free(header);
                    close(socket_desc);
                    return -1;
                }

            }else if(headBytesRd >= buffer_size)
            {
                gfr->status = GF_INVALID;
                free(buffer);
                free(header);
                close(socket_desc);
                return -1;
            }



        }else{
            if(gfr->bytesreceived + bytesRd >= gfr-> filelen){
                gfr->writefunc(buffer, gfr->filelen - gfr->bytesreceived, gfr->writearg);
                gfr->bytesreceived = gfr->filelen;
                free(header);
                free(buffer);
                close(socket_desc);
                return 0;
            }

            gfr->writefunc(buffer, (size_t)bytesRd, gfr->writearg);
            gfr->bytesreceived += bytesRd;


        }


    }

}



void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    gfr->headerarg = headerarg;

}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
    gfr->headerfunc = headerfunc;

}

void gfc_set_path(gfcrequest_t *gfr, char* path){
    strcpy(gfr->path, path);
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
    gfr->port = port;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
    strcpy(gfr->server, server);
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    gfr->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    gfr->writefunc = writefunc;
}

char* gfc_strstatus(gfstatus_t status){

    if(status == GF_OK)
        return "OK";
    else if(status == GF_FILE_NOT_FOUND)
        return "FILE_NOT_FOUND";
    else if(status == GF_ERROR)
        return "ERROR";
    else
        return "INVALID";

}

