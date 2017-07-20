//
// Created by dalaoshe on 17-7-16.
//

#ifndef TCP_TUNNEL_SCHEDULE_H
#define TCP_TUNNEL_SCHEDULE_H


#include <stdint-gcc.h>

#include "ofmsg.h"
#include <queue>
#include <vector>
#include <cstdio>
#include <pthread.h>

using namespace std;

struct OFP_Msg_Arg {
    uint8_t qid;
    uint8_t priority;
    int32_t max_wait_time;
    int32_t process_time;
};

class OFP_Msg {
public:
    int32_t len;
    int32_t priority;
    int32_t fd;
    int32_t max_wait_time;// indicate how long can wait
    int32_t process_time;// indicate processed finished
    uint8_t identity;
    uint8_t in_process;
    uint8_t finished;
    rofl::openflow::ofp_header *header;
    char* buf;
public:
    OFP_Msg(char* msg, int32_t len, int32_t fd, int32_t priority) {
        this->len = len;
        this->priority = priority;
        this->fd = fd;
        this->identity = 0;
        this->in_process = 0;
        this->finished = 0;


        this->buf = new char[len];
        //memset(buf, 0, len);
        this->buf = new char [len];
        for(int i = 0 ; i < len; ++i) {
            this->buf[i] = msg[i];
        }
        header = (rofl::openflow::ofp_header *)buf;



    }
    ~OFP_Msg() {
        delete this->buf;
    }
//    bool operator < (const OFP_Msg &t1, const OFP_Msg &t2);
};
// if t1 priority < t2
bool lessPriority (const OFP_Msg &t1, const OFP_Msg &t2);


//switch to controller
#define PI_QUEUE_ID 0
#define MSG_QUEUE_ID 2
//controller to switch
#define FLOW_MOD_QUEUE_ID 0
#define PO_QUEUE_ID 1


class Queue {
    pthread_mutex_t queue_mutex;
    //std::queue<OFP_Msg*> msg_queue;
    std::vector<OFP_Msg*> msg_queue;
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
        this->msg_queue.push_back(msg);
        this->size ++;
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }
    int32_t decMsgTime() {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
//        if(num > 0) {
//            fprintf(stderr, "\n\n dec time %u \n\n", num);
//        }
        for(uint64_t i = 0; i < num; ++i) {
            this->msg_queue[i]->max_wait_time --;
            if(this->msg_queue[i]->in_process) {
                this->msg_queue[i]->process_time --;
                if(this->msg_queue[i]->process_time < 0) {
                    this->msg_queue[i]->finished = 1;
                }
              //  fprintf(stderr, "\n\n need time %d \n\n", this->msg_queue[i]->process_time);
            }
        }
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }

    //every call mark one only
    int32_t markFinished(uint8_t identify) {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
        if(num > 0) {
            fprintf(stderr, "\n\n identify %u \n\n", identify);
        }
        for(uint64_t i = 0; i < num; ++i) {
            if(this->msg_queue[i]->in_process
               && !this->msg_queue[i]->finished
               && this->msg_queue[i]->identity == identify) {
                this->msg_queue[i]->finished = 1;
                pthread_mutex_unlock(&this->queue_mutex);
                return 1;
                //  fprintf(stderr, "\n\n need time %d \n\n", this->msg_queue[i]->process_time);
            }
        }
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }

    //pop and get number of finished msgs
    int32_t cleanFinishMsg() {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
        int32_t count = 0;
        for(uint64_t i = 0,index = 0; i < num; ++i,++index) {
            //fprintf(stderr, "\n\n packet need time %d in process %d \n\n", this->msg_queue[index]->process_time, this->msg_queue[index]->in_process);

            if((this->msg_queue[index]->in_process) && (this->msg_queue[index]->finished)) {
                delete this->msg_queue[index];
                this->msg_queue.erase(this->msg_queue.begin() + index);
                --index;
                ++count;
            }
            else {
//                fprintf(stderr, "\n\n packet need time %d in process %d \n\n", this->msg_queue[index]->process_time, this->msg_queue[index]->in_process);
            }
            //fprintf(stderr, "\n\n clean count %d \n\n", count);
        }


        pthread_mutex_unlock(&this->queue_mutex);
        return count;
    }
    //get max priority msg who not in process
    OFP_Msg* fetchMsg() {
        pthread_mutex_lock(&this->queue_mutex);
        OFP_Msg* msg = NULL;
        uint64_t num = this->msg_queue.size();
        for(uint64_t i = 0; i < num; ++i) {
            if(!this->msg_queue[i]->in_process) {
                if(msg == NULL)  {
                    msg = this->msg_queue[i];
                    continue;
                }
                else if(lessPriority((*msg) , *(msg_queue[i]))){
                    msg = msg_queue[i];
                }
            }
        }
        pthread_mutex_unlock(&this->queue_mutex);
        return msg;
    }
    Queue() {
        this->queue_mutex = PTHREAD_MUTEX_INITIALIZER;
        this->size = 0;
    }
};

#define MAX_MATCH_LEN 256
#define POLICY_ADD 0
#define POLICY_RESPONSE 1
struct Policy {
    struct hdr{
        int32_t match_len;
        int32_t priority;
    }h;
    char match[MAX_MATCH_LEN];
};

#define MAX_POLICY_MSG_DATA 1024
struct PolicyMsg {
    int8_t type;
    int8_t policy_num;
    int16_t byte_len;
    char data[MAX_POLICY_MSG_DATA];
};

#define CONFIG_PORT 5656
class PolicyConfig {
    sockaddr_in server;
    int32_t config_fd;
    std::vector<Policy*> policies;
    pthread_mutex_t policy_mutex = PTHREAD_MUTEX_INITIALIZER;
public:
    int32_t setupConf();
    int32_t listenRequest();
    std::vector<Policy*>* getAllPolices() {
        return &(this->policies);
    }
};

class Schedule {
    Queue pi_queue;
    Queue msg_queue;
    std::vector<Queue*> queues;
    pthread_mutex_t policy_mutex;
    PolicyConfig* conf;
    int32_t queue_num;
    char resp_pipe[50];
    char name[30];
   // int32_t fd;
public:
    Schedule(char* name, char* pipe_name) {
        policy_mutex = PTHREAD_MUTEX_INITIALIZER;
        strcpy(this->name, name);
        strcpy(this->resp_pipe, pipe_name);
        fprintf(stderr, "\nCreate Schedule:%s , pipe:%s\n",this->name, this->resp_pipe);
        this->queue_num = 10;
        for(int i = 0; i < this->queue_num; ++i) {
            Queue* q = new Queue();
            queues.push_back(q);
        }
     //   this->fd = fd;
    }
    int32_t putMessage(char* msg, int32_t len, int32_t fd);
    int32_t run();
    int32_t addPolicy(Policy* policy);
    int32_t getPriority(char* msg);
    int32_t getQueueId(char* msg);
    int32_t getMaxWaitTime(char* msg);
    int32_t getProcessTime(char* msg);
    OFP_Msg_Arg getOFPMsgArg(char* msg);
    void setConf(PolicyConfig* conf) {
        this->conf = conf;
    }
    ~Schedule() {

    }
};

struct ScheduleArg{
    Schedule* schedule;
    int32_t* fd;
    PolicyConfig* config;
    //int32_t* server_fd;
};
void* schedule_thread(void* argv) ;

#endif //TCP_TUNNEL_SCHEDULE_H
