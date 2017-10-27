/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */

typedef opaque img<>;



program RESIZE_PROG{
    version RESIZE_PROG_VERS{
        img RESIZE_IMAGE(img) = 1;
    } = 1;
} = 0x20000079;
