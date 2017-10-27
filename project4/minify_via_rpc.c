#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"
#include "minifyjpeg.h"

CLIENT* get_minify_client(char *server){
    CLIENT *cl;

    /* Your code here */
    if((cl = clnt_create(server, RESIZE_PROG, RESIZE_PROG_VERS, "tcp")) == NULL){
        printf("clnt_create error");
        exit(1);
    }

    //printf("clnt_create successful");
    return cl;
}


void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len){

	/*Your code here */
    printf("start minify_via_rpc, src_len: %zu\n", src_len);

    img *src = (img*)malloc(sizeof(img));
    img *dst;


    src->img_val = (char *) src_val;
    src->img_len = (int)src_len;


    if((dst = resize_image_1(*src, cl)) == NULL){
        printf("call resize_image_1 error");
        exit(1);
    }

    *dst_len = dst->img_len;


    printf("2. the dst_len: %d\n", dst->img_len);

    return (void *)dst->img_val;
 
}