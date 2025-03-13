//
// Created by root on 2/11/25.
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "cron_dll.h"

struct doubly_linked_list_t* dll_create(){
    struct doubly_linked_list_t *dll = (struct doubly_linked_list_t*)calloc(1,sizeof(struct doubly_linked_list_t));
    if(dll==NULL) return dll;
    dll->head=NULL;
    dll->tail=NULL;
    return dll;
}

struct node_t* dll_push_back(struct doubly_linked_list_t* dll, int value, int id){
    if(!dll) return NULL;
    if((dll->head==NULL && dll->tail!=NULL) || (dll->head!=NULL && dll->tail==NULL) ) return NULL;
    if(dll->head==NULL){
        dll->head = (struct node_t*) calloc(1,sizeof(struct node_t));
        if(dll->head==NULL) return NULL;
        dll->tail=dll->head;
        dll->head->data = value;
        dll->head->is_active = 1;
        dll->head->id = id;
        dll->head->next = NULL;
        dll->head->prev = NULL;
        return dll->head;
    }else{
        struct node_t *new = calloc(1,sizeof(struct node_t));
        if(!new){
            //free
            return NULL;
        }
        new->prev=dll->tail;
        new->data=value;
        new->is_active = 1;
        new->id = id;
        dll->tail->next=new;
        dll->tail=new;
        return new;
    }
    return NULL;
}
int dll_pop_front(struct doubly_linked_list_t* dll, int *err_code){
    if(!dll){
        if(err_code) *err_code=1;
        return 0;
    }
    if(dll->head==NULL || dll->tail==NULL ){
        if(err_code) *err_code = 1;
        return 0;
    }
    if(dll->head==dll->tail){
        int value=dll->head->data;
        dll->head->is_active = 0;
        timer_delete(dll->head->timer);
        if(err_code) *err_code=0;
        return value;
    }
    int value=dll->head->data;
    dll->head->is_active = 0;
    timer_delete(dll->head->timer);
    if(err_code) *err_code=0;
    return value;

}
int dll_pop_back(struct doubly_linked_list_t* dll, int *err_code){
    if(!dll){
        if(err_code) *err_code=1;
        return 0;
    }
    if(dll->head==NULL || dll->tail==NULL ){
        if(err_code) *err_code = 1;
        return 0;
    }
    if(dll->head==dll->tail){
        int value=dll->tail->data;
        dll->tail->is_active = 0;
        timer_delete(dll->tail->timer);
        if(err_code) *err_code=0;
        return value;
    }
    int value=dll->tail->data;
    dll->tail->is_active = 0;
    timer_delete(dll->tail->timer);
    if(err_code) *err_code=0;
    return value;
}

int dll_size(const struct doubly_linked_list_t* dll){
    if(!dll) return -1;
    if(!dll->head) return 0;
    struct node_t *temp = dll->head;
    int i=1;
    while(temp!= dll->tail){
        if(temp->next==NULL) return -1;
        temp = temp->next;
        ++i;
    }
    return i;
}

int dll_remove(struct doubly_linked_list_t* dll, unsigned int index, int *err_code){
    if(!dll){
        if(err_code) *err_code=1;
        return 1;
    }
    if(dll_size(dll)==-1){
        if(err_code) *err_code=1;
        return 1;
    }
    if(index==0){
        int value= dll_pop_front(dll,err_code);
        return value;
    }
    if(index==(unsigned int) dll_size(dll)-1){
        int value= dll_pop_back(dll,err_code);
        return value;
    }
    unsigned int i=0;
    struct node_t *node=dll->head;
    for(;i<index;++i){
        node=node->next;
        if(node==NULL){
            if(err_code) *err_code=1;
            return 1;
        }
    }
    node->is_active = 0;
    timer_delete(node->timer);
    if(err_code) *err_code=0;
    return 0;
}

void dll_clear(struct doubly_linked_list_t* dll){
    if(dll){
        struct node_t *node=dll->head;
        struct node_t *temp;
        while(node){
            temp=node;
            timer_delete(node->timer);
            node=node->next;
            free(temp);
        }
        dll->head=NULL;
        dll->tail=NULL;
    }
}