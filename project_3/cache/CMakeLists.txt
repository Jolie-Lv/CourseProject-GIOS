cmake_minimum_required(VERSION 3.7)
project(cache)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        gfclient_download
        gfclient_measure
        gfserver.h
        gfserver.o
        gfserver_noasan.o
        handle_with_cache.c
        handle_with_curl.c
        handle_with_file.c
        locals.txt
        Makefile
        shm_channel.c
        shm_channel.h
        simplecache.c
        simplecache.h
        simplecached.c
        steque.c
        steque.h
        webproxy.c
        workload.txt)

add_executable(cache ${SOURCE_FILES})