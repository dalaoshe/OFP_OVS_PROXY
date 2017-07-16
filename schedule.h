//
// Created by dalaoshe on 17-7-16.
//

#ifndef TCP_TUNNEL_SCHEDULE_H
#define TCP_TUNNEL_SCHEDULE_H


#include <stdint-gcc.h>

#include "ofmsg.h"
#include <queue>
#include <cstdio>
#include <pthread.h>

using namespace std;

class OFP_Msg {
public:
    int32_t len;
    int32_t priority;
    int32_t fd;
    char* buf;
public:
    OFP_Msg(char* msg, int32_t len, int32_t fd, int32_t priority) {
        this->buf = new char [len];
        memset(this->buf, 0, len);
        this->len = len;
        for(int i = 0 ; i < len; ++i) {
            this->buf[i] = msg[i];
        }
//        strncpy(this->buf, msg, len);
        this->priority = priority;
        this->fd = fd;
    }
    ~OFP_Msg() {
        delete this->buf;
    }
};

#define PI_QUEUE_ID 0
#define MSG_QUEUE_ID 1

class Queue {
    pthread_mutex_t queue_mutex;
    std::queue<OFP_Msg*> msg_queue;
    int32_t size;
public:
    int32_t getSize() {
        pthread_mutex_lock(&this->queue_mutex);
        int32_t temp = this->size;
        pthread_mutex_unlock(&this->queue_mutex);
        return temp;
    }
    int32_t putMsg(OFP_Msg* msg) {
        pthread_mutex_lock(&this->queue_mutex);
        this->msg_queue.push(msg);
        this->size ++;
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }
    OFP_Msg* fetchMsg() {
        pthread_mutex_lock(&this->queue_mutex);
        OFP_Msg* msg = NULL;
        if(!this->msg_queue.empty()) {
            msg = msg_queue.front();
            msg_queue.pop();
        }
        else {

        }
        pthread_mutex_unlock(&this->queue_mutex);
        return msg;
    }
    Queue() {
        this->queue_mutex = PTHREAD_MUTEX_INITIALIZER;
        this->size = 0;
    }
};

class Policy {

};

class Schedule {
    Queue pi_queue;
    Queue msg_queue;
    pthread_mutex_t policy_mutex;
    std::vector<Policy*> policies;
    char name[30];
   // int32_t fd;
public:
    Schedule(char* name) {
        policy_mutex = PTHREAD_MUTEX_INITIALIZER;
        strcpy(this->name, name);
     //   this->fd = fd;
    }
    int32_t putMessage(char* msg, int32_t len, int32_t fd);
    int32_t run();
    int32_t addPolicy(Policy* policy);
    int32_t getPriority(char* msg);
    int32_t getQueueId(char* msg);
    ~Schedule() {

    }
};

struct ScheduleArg{
    Schedule* schedule;
    int32_t* fd;
    //int32_t* server_fd;
};
void* schedule_thread(void* argv) ;

#endif //TCP_TUNNEL_SCHEDULE_H
