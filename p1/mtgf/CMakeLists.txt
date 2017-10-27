cmake_minimum_required(VERSION 3.7)
project(mtgf)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        server_root/courses/ud923/filecorpus/1kb-sample-file-0.png
        server_root/courses/ud923/filecorpus/1kb-sample-file-1.html
        server_root/courses/ud923/filecorpus/1kb-sample-file-2.png
        server_root/courses/ud923/filecorpus/1kb-sample-file-3.jpg
        server_root/courses/ud923/filecorpus/1kb-sample-file-4.png
        server_root/courses/ud923/filecorpus/moranabovejacksonlake.jpg
        server_root/courses/ud923/filecorpus/paraglider.jpg
        server_root/courses/ud923/filecorpus/road.jpg
        server_root/courses/ud923/filecorpus/yellowstone.jpg
        content.c
        content.h
        content.txt
        gfclient.h
        gfclient.o
        gfclient_download.c
        gfserver.h
        gfserver.o
        gfserver_main.c
        handler.c
        log.h
        Makefile
        steque.c
        steque.h
        workload.c
        workload.h
        workload.txt)

add_executable(mtgf ${SOURCE_FILES})