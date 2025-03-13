#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <spawn.h>
#include <errno.h>
#include "cron_dll.h"
#include "logger.h"


#define SHM_INFO "/shm_info"

#define SERVER_RECEIVE_QUEUE "/mq_queries_server"

struct server_info_t{
    pid_t pid;
};

enum operation_t{ADD,REMOVE,SHOW,QUIT};
enum reply_status_t{PROCESSING,DONE,ERROR, EMPTY};

struct query_t{
    char reply_queue_name[256];
    int absolute;
    int time;
    int interval;
    enum operation_t operation;
    int index;
    int argc;
    char command[20][256];
};

struct reply_t{
    enum reply_status_t status;
    struct node_t packet;
};

pthread_mutex_t terminate_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dll_mutex = PTHREAD_MUTEX_INITIALIZER;

mqd_t mq_queries_from_clients;


struct doubly_linked_list_t *dll = NULL;

int id=0;


void* execute_task(void* arg){
    struct node_t *task = (struct node_t*)arg;

    if(task->argc>=0){
        pid_t pid;
        char **args = (char**)calloc(task->argc+1,sizeof(char*));
        for(int i=0;i<=task->argc;++i){
            args[i] = strdup(task->command[i]);
        }
        args[task->argc] = NULL;
        posix_spawn(&pid, args[0], NULL, NULL,args, NULL);
        if(task->interval==0){
            timer_delete(task->timer);
            task->is_active = 0;
        }

        for(int i=0;args[i]!=NULL;++i){
            free(args[i]);
        }
        free(args);
        int status;
        do
            waitpid(pid, &status, 0);
        while(!WIFEXITED(status));
    }
    return NULL;
}

void* processing(void* arg){
    int end = 0;
    struct query_t query;
    struct reply_t reply;
    struct node_t *node;
    while(!end){
        mq_receive(mq_queries_from_clients, (char*)&query, sizeof(struct query_t),NULL);

        mqd_t mq_query_to_client = mq_open(query.reply_queue_name,O_WRONLY);
        if(mq_query_to_client==-1){
            printf("Couldn't open client message queue\n");
            return NULL;
        }

        pthread_mutex_lock(&terminate_lock);

        switch (query.operation) {

            case ADD:
                pthread_mutex_lock(&dll_mutex);
                node=dll_push_back(dll,query.time,id);
                id+=1;


                reply.packet = *dll->head;
                reply.status = DONE;


                node->argc = query.argc;
                node->interval = query.interval;
                for(int i=0;i<=query.argc;++i){
                    strcpy(node->command[i],query.command[i]);
                }


                struct sigevent timer_event;

                timer_event.sigev_notify = SIGEV_THREAD;
                timer_event.sigev_notify_function = execute_task;
                timer_event.sigev_value.sival_ptr = node;
                timer_event.sigev_notify_attributes = NULL;

                timer_create(CLOCK_REALTIME,&timer_event,&(node->timer));

                struct itimerspec value;

                value.it_value.tv_sec = query.time;
                value.it_value.tv_nsec = 0;
                value.it_interval.tv_sec = query.interval;
                value.it_interval.tv_nsec = 0;

                if(query.absolute==1){
                    timer_settime(node->timer,TIMER_ABSTIME,&value,NULL);
                }else if(query.absolute==0){
                    timer_settime(node->timer,0,&value,NULL);
                }

                pthread_mutex_unlock(&dll_mutex);
                log_to_file(MAX,"Task %d created\n",node->id);
                break;
            case REMOVE:
                pthread_mutex_lock(&dll_mutex);
                dll_remove(dll,query.index,NULL);
                pthread_mutex_unlock(&dll_mutex);
                log_to_file(STANDARD,"Task %d deleted\n",query.index);
                break;
            case SHOW:
                pthread_mutex_lock(&dll_mutex);
                node = dll->head;
                if(node==NULL){
                    reply.status = EMPTY;
                }
                while(node!=NULL){
                    reply.packet = *node;
                    reply.status = PROCESSING;
                    if(node->next==NULL)
                        reply.status = DONE;
                    mq_send(mq_query_to_client,(const char* )&reply,sizeof(struct reply_t),0);
                    node = node->next;
                }
                pthread_mutex_unlock(&dll_mutex);
                log_to_file(MIN,"Tasks were shown\n");
                break;
            case QUIT:
                end = 1;
                reply.status = DONE;
                mq_send(mq_query_to_client,(const char*)&reply,sizeof(struct reply_t),0);
                mq_close(mq_query_to_client);
                log_to_file(MAX,"Server closed\n");
                pthread_mutex_unlock(&terminate_lock);
                continue;
        }
        pthread_mutex_unlock(&terminate_lock);

        mq_send(mq_query_to_client,(const char* )&reply,sizeof(struct reply_t),0);
        mq_close(mq_query_to_client);
    }
    return NULL;
}

int main(int argc, char** argv) {
    setvbuf(stdout,NULL,_IONBF,BUFSIZ);
    int info_id = shm_open(SHM_INFO,O_RDONLY,0666);
    ftruncate(info_id,sizeof(struct server_info_t));
    struct server_info_t *server_info = mmap(NULL,sizeof(struct server_info_t),PROT_READ,MAP_SHARED,info_id,0);
    if(info_id == -1 || server_info == MAP_FAILED || kill(server_info->pid,0)==-1){
        mq_unlink(SERVER_RECEIVE_QUEUE);
        //SERVER
        if(info_id != -1){
            munmap(server_info,sizeof(struct server_info_t));
            close(info_id);
            shm_unlink(SHM_INFO);
        }
        info_id = shm_open(SHM_INFO,O_CREAT | O_EXCL | O_RDWR,0666);
        if(info_id == -1){
            printf("shm_open fail\n");
            return 1;
        }
        ftruncate(info_id,sizeof(struct server_info_t));
        server_info = mmap(NULL,sizeof(struct server_info_t),PROT_READ | PROT_WRITE,MAP_SHARED,info_id,0);
        if(server_info == MAP_FAILED){
            printf("mmap fail\n");
            close(info_id);
            return 2;
        }
        server_info->pid = getpid();
        munmap(server_info,sizeof(struct server_info_t));
        close(info_id);

        struct mq_attr attr;

        attr.mq_maxmsg = 10;
        attr.mq_msgsize = sizeof(struct query_t);
        attr.mq_flags = 0;

        mq_queries_from_clients = mq_open(SERVER_RECEIVE_QUEUE,O_CREAT | O_RDONLY,0666,&attr);

        dll = dll_create();

        pthread_t processing_thread;

        pthread_create(&processing_thread,NULL,processing,NULL);
        init_logger("logs.txt");

        printf("Server started PID: %d\n", getpid());

        pthread_join(processing_thread,NULL);


        dll_clear(dll);
        close_logger();


        mq_close(mq_queries_from_clients);
        mq_unlink(SERVER_RECEIVE_QUEUE);

        shm_unlink(SHM_INFO);

        printf("Server terminated PID: %d\n",getpid());



    }else{
        //CLIENT
        printf("Client started PID: %d\n",getpid());

        mqd_t mq_queries_to_server;

        do{
            mq_queries_to_server = mq_open(SERVER_RECEIVE_QUEUE,O_WRONLY);
            sleep(1);
        }while(mq_queries_to_server == -1);

        struct query_t query;

        sprintf(query.reply_queue_name,"/mq_reply_name_%d",getpid());

        struct mq_attr attr;

        attr.mq_maxmsg = 10;
        attr.mq_msgsize = sizeof(struct reply_t);
        attr.mq_flags = 0;

        mqd_t mq_reply_from_server = mq_open(query.reply_queue_name,O_CREAT | O_RDONLY,0666,&attr);

        struct reply_t reply;

        if(argc>1){

            if(strcmp(argv[1],"add")==0){
                query.operation = ADD;
                char *endptr;
                if(strcmp(argv[2],"-r")==0){
                    query.time = strtol(argv[3],&endptr,10);
                    query.interval = strtol(argv[4],&endptr,10);
                    int i=0;
                    for(int j=5;j<argc;++j){
                        strcpy(query.command[i],argv[j]);
                        ++i;
                    }
                    (query.command[i][0]) = (char) NULL;
                    query.argc = i;
                    query.absolute=0;
                }
                if(strcmp(argv[2],"-a")==0){
                    struct tm temp;
                    temp.tm_sec = strtol(argv[3],&endptr,10);
                    temp.tm_min = strtol(argv[4],&endptr,10);
                    temp.tm_hour = strtol(argv[5],&endptr,10);
                    temp.tm_mday = strtol(argv[6],&endptr,10);
                    temp.tm_mon = strtol(argv[7],&endptr,10);
                    temp.tm_year = strtol(argv[8],&endptr,10);
                    query.interval = strtol(argv[9],&endptr,10);
                    temp.tm_mon-=1;
                    temp.tm_year-=1900;
                    query.time = mktime(&temp);
                    int i=0;
                    for(int j=10;j<argc;++j){
                        strcpy(query.command[i],argv[j]);
                        ++i;
                    }
                    (query.command[i][0]) = (char) NULL;
                    query.argc = i;
                    query.absolute=1;
                }

            }
            if(strcmp(argv[1],"show")==0){
                query.operation = SHOW;
                mq_send(mq_queries_to_server,(const char* )&query, sizeof(struct query_t),0);
                do{
                    mq_receive(mq_reply_from_server,(char* )&reply,sizeof(struct reply_t),NULL);
                    if(reply.status==EMPTY) {
                        printf("There are no tasks\n");
                        break;
                    }
                    if(reply.status==ERROR){
                        printf("Error\n");
                        break;
                    }
                    printf("Task id: %d ",reply.packet.id);
                    reply.packet.is_active == 1 ? printf("active \n") : printf("inactive \n");
                }while(reply.status == PROCESSING);

                mq_close(mq_queries_to_server);
                mq_close(mq_reply_from_server);
                mq_unlink(query.reply_queue_name);

                printf("Client terminated PID: %d\n",getpid());
                return 0;
            }
            if(strcmp(argv[1],"remove")==0){
                char *endptr;
                int index = strtol(argv[2],&endptr,10);
                query.index=index;
                query.operation = REMOVE;
            }
            if(strcmp(argv[1],"quit")==0){
                query.operation = QUIT;
                mq_send(mq_queries_to_server,(const char* )&query, sizeof(struct query_t),0);
                mq_receive(mq_reply_from_server,(char*)&reply,sizeof(struct reply_t),NULL);
                mq_close(mq_queries_to_server);
                mq_close(mq_reply_from_server);
                mq_unlink(query.reply_queue_name);

                printf("Client terminated PID: %d\n",getpid());
                return 0;
            }
        }

        mq_send(mq_queries_to_server,(const char* )&query, sizeof(struct query_t) ,0);

        mq_receive(mq_reply_from_server,(char* )&reply,sizeof(struct reply_t),NULL);


        mq_close(mq_queries_to_server);
        mq_close(mq_reply_from_server);
        mq_unlink(query.reply_queue_name);

        printf("Client terminated PID: %d\n",getpid());

    }
    return 0;
}
