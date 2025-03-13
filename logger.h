//
// Created by root on 11/8/24.
//

#ifndef LOGGER_LOGGER_H
#define LOGGER_LOGGER_H

#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>

enum detail_level_t {MIN=0, STANDARD=1, MAX=2};

void log_to_file(int priority, const char* format, ...);
void close_logger();
void init_logger(const char* log_filename);

#endif //LOGGER_LOGGER_H
