cmake_minimum_required(VERSION 4.0)
project(AsyncServer)

set(CMAKE_CXX_STANDARD 20)

add_executable(AsyncServer main.cpp
        App/AsyncServer.cpp
        App/AsyncServer.h
        App/CommandProcessor.cpp
        App/CommandProcessor.h
        App/ServerStats.h
)
