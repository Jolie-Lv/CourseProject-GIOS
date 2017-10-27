cmake_minimum_required(VERSION 3.7)
project(gflib)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        content.c
        content.h
        gfclient.c
        gfclient.h
        gfclient_download.c
        gfserver.c
        gfserver.h
        gfserver_main.c
        handler.c
        workload.c
        workload.h)

add_executable(gflib ${SOURCE_FILES})