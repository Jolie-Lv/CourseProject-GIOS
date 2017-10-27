cmake_minimum_required(VERSION 3.7)
project(echo)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        echoclient.c
        echoserver.c)

add_executable(echo ${SOURCE_FILES})