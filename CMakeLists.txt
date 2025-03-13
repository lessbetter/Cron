cmake_minimum_required(VERSION 3.30)
project(Cron C)

set(CMAKE_C_STANDARD 11)

add_executable(Cron main.c
        cron_dll.h
        cron_dll.c
        logger.h
        logger.c)
target_link_libraries(Cron m pthread rt)