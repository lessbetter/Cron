//
// Created by root on 11/8/24.
//
#include <stdatomic.h>
#include "logger.h"

static atomic_int detail_level = MIN;
static volatile sig_atomic_t log_on = 0; //0 - off;  1 - on;

static FILE* log_file;

static int is_init=0;

static pthread_t dump_thread;

static sem_t sem_dump;

static pthread_mutex_t log_file_operations;

void signal_handler(int signo, siginfo_t* info, void* other){
    if(signo == SIGRTMIN){
        detail_level = info->si_value.sival_int;
    }else if(signo == SIGUSR1){
        log_on = info->si_value.sival_int;
    }else if(signo == SIGUSR2){
        sem_post(&sem_dump);
    }
}

void log_to_file(int priority, const char* format, ...){
    if(is_init!=1) return;
    if(log_file==NULL) return;
    if(log_on!=1) return;
    pthread_mutex_lock(&log_file_operations);
    time_t now;
    time(&now);
    struct tm* tm = localtime(&now);
    va_list args;
    va_start(args, format);
    switch (priority) {
        case MIN:
            vfprintf(log_file,format,args);
            fprintf(log_file,"\n");
            fflush(log_file);
            break;
        case STANDARD:
            fprintf(log_file,"%02d:%02d:%02d\t",tm->tm_hour,tm->tm_min,tm->tm_sec);
            vfprintf(log_file,format,args);
            fprintf(log_file,"\n");
            fflush(log_file);
            break;
        case MAX:
            fprintf(log_file,"%d-%02d-%d\t%02d:%02d:%02d\t",
                    tm->tm_mday,tm->tm_mon+1,tm->tm_year+1900,
                    tm->tm_hour,tm->tm_min,tm->tm_sec);
            vfprintf(log_file,format,args);
            fprintf(log_file,"\n");
            fflush(log_file);
            break;
        default:
            printf("Incorrect detail level\n");
            break;
    }
    va_end(args);
    pthread_mutex_unlock(&log_file_operations);
}

void* dump_fun(void* arg){
    while(1){
        sem_wait(&sem_dump);
        char filename[20] = "dump_XXXXXX";
        mkstemp(filename);
        FILE *fp = fopen(filename,"wt");
        if(fp==NULL){
            close_logger();
            return NULL;
        }
        time_t now;
        time(&now);
        struct tm* tm = localtime(&now);
        fprintf(fp,"File created: %d-%02d-%d %02d:%02d:%02d\n",
                tm->tm_mday,tm->tm_mon+1,tm->tm_year+1900,
                tm->tm_hour,tm->tm_min,tm->tm_sec);
        fprintf(fp,"Logging: ");
        log_on == 1 ? fprintf(fp,"on\n"): fprintf(fp,"off\n");
        switch (detail_level) {
            case MIN:
                fprintf(fp,"Details level: min\n");
                break;
            case STANDARD:
                fprintf(fp,"Details level: standard\n");
                break;
            case MAX:
                fprintf(fp,"Details level: max\n");
                break;
            default:
                fprintf(fp,"Error: Details level undefined\n");
                break;
        }
        fclose(fp);
    }
}

void init_logger(const char* log_filename){

    if(is_init==1)
        return;

    is_init=1;
    log_on = 1;

    log_file = fopen(log_filename,"a");
    if(log_file == NULL){
        perror("Couldn't open file\n");
        return;
    }

    pthread_mutex_init(&log_file_operations, NULL);

    sem_init(&sem_dump,0,0);

    pthread_create(&dump_thread,NULL,dump_fun,NULL);

    struct sigaction act;

    sigset_t set;
    sigemptyset(&set);

    act.sa_sigaction = signal_handler;
    act.sa_mask = set;
    act.sa_flags = SA_SIGINFO;

    sigaction(SIGRTMIN,&act,NULL);
    sigaction(SIGUSR1,&act,NULL);
    sigaction(SIGUSR2,&act,NULL);
}

void close_logger(){
    is_init = 0;
    pthread_cancel(dump_thread);
    sem_destroy(&sem_dump);
    pthread_mutex_destroy(&log_file_operations);
    fclose(log_file);
    log_on = 0;
    detail_level = MIN;
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGRTMIN,&sa,NULL);
    sigaction(SIGUSR1,&sa,NULL);
    sigaction(SIGUSR2,&sa,NULL);
}