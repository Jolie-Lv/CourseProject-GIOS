#include "minifyjpeg.h"
#include "magickminify.h"

/* Implement the needed server-side functions here */

img * resize_image_1_svc(img args, struct svc_req *rqstp ){
    static img dst;

    printf("The image size is: %d\n", (int)args.img_len);

    magickminify_init();
    dst.img_val = (char *)magickminify((void *)args.img_val, (size_t)args.img_len, (ssize_t *)&dst.img_len);



    printf("The resized image size is: %d\n", dst.img_len);

    magickminify_cleanup();

    return &dst;

}
