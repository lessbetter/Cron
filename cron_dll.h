//
// Created by root on 2/11/25.
//

#ifndef CRON_CRON_DLL_H
#define CRON_CRON_DLL_H

struct doubly_linked_list_t
{
    struct node_t *head;
    struct node_t *tail;
};

struct node_t
{
    int interval;
    int data;
    int id;
    timer_t timer;
    int is_active;
    struct node_t *next;
    struct node_t *prev;
    int argc;
    char command[20][256];
};

struct doubly_linked_list_t* dll_create();

struct node_t* dll_push_back(struct doubly_linked_list_t* dll, int value, int id);


int dll_remove(struct doubly_linked_list_t* dll, unsigned int index, int *err_code);

void dll_clear(struct doubly_linked_list_t* dll);


#endif //CRON_CRON_DLL_H
