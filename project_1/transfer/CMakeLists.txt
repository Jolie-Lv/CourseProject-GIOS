cmake_minimum_required(VERSION 3.7)
project(transfer)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        transferclient.c
        transferserver.c)

add_executable(transfer ${SOURCE_FILES})