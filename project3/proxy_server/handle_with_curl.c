#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

struct MemoryStruct{
    char * memory;
    size_t size;
};

size_t write_data(void *ptr, size_t size, size_t nmemb, void *userp){
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL)
        return 0;
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;

}

ssize_t handle_with_curl(gfcontext_t *ctx, char *path, void* arg){
    ssize_t bytes_transferred = 0;

	CURL *curl_handle;
    CURLcode res;
    long response_code = 0;
    struct MemoryStruct body;
    body.memory = malloc(1);
    body.size = 0;

    char buffer[4096];
    char *data_dir = arg;

    strcpy(buffer,data_dir);
    strcat(buffer,path);

    curl_global_init(CURL_GLOBAL_ALL);

    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, buffer);

    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&body);

    res = curl_easy_perform(curl_handle);

    curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &response_code);

    if (response_code == 200 && res != CURLE_ABORTED_BY_CALLBACK)
    {
        gfs_sendheader(ctx, GF_OK, body.size);
        bytes_transferred = gfs_send(ctx, body.memory, body.size);
        if (bytes_transferred != body.size){
            return SERVER_FAILURE;
        }

    }
    else
    {
        curl_easy_cleanup(curl_handle);
        free(body.memory);
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

    }



    curl_easy_cleanup(curl_handle);
    free(body.memory);

    return bytes_transferred;

}

