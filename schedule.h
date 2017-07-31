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
#include <cstring>
#include <string.h>
#include <map>
#include "PriorityManager.h"
using namespace std;
std::string getOFPMsgType(uint8_t ofp_type);
struct OFP_Msg_Arg {
    uint8_t qid;
    double_t priority;
    int32_t max_wait_time;
    int32_t process_time;
    uint64_t identify;
    uint16_t len;
    Split::UEPID uepid;

};

class OFP_Msg {
public:
    int32_t len; /* Valid Payload Len To Sent */
    double_t priority /* Priority */;
    int32_t fd; /* The FD To Write */
    int32_t max_wait_time;/* indicate how long can wait */
    int32_t process_time;/* indicate processed finished */
    uint64_t identity;/* Identity To Receive From SW */
    Split::UEPID uepid;/* UEP To Get Priority */
    uint8_t in_process;/* Has Sent But Not Be Finished */
    uint8_t finished;/* Sent And Finished Wait To Clean */
    timeval recv, send, end;/* timestamp */
    uint8_t ofp_type;/* OFP Type */
    uint8_t read_to_be_sent;/* Be Ready To Be Sent */
    rofl::openflow::ofp_header *header;
    char* buf;/* Payload */
public:
    OFP_Msg(char* msg, int32_t len, int32_t fd, double_t priority) {
        this->len = len;
        this->priority = priority;
        this->fd = fd;
        this->identity = 0;
        this->read_to_be_sent = 1;
        this->uepid.eid = 0;
        this->uepid.uid = 0;

        this->in_process = 0;
        this->finished = 0;
        gettimeofday(&recv, NULL);

        //this->buf = new char[len];
        //memset(buf, 0, len);
        this->buf = new char [len];
        for(int i = 0 ; i < len; ++i) {
            this->buf[i] = msg[i];
        }
        this->header = (rofl::openflow::ofp_header *)(this->buf);
        this->ofp_type = ((rofl::openflow::ofp_header *)(this->buf))->type;



    }
    ~OFP_Msg() {
        delete this->buf;
    }
};
// if t1 priority < t2
bool lessPriority (const OFP_Msg &t1, const OFP_Msg &t2);


//switch to controller
#define PI_QUEUE_ID 1
#define MULTI_PART_REPLY_QUEUE_ID 1
#define MSG_QUEUE_ID 2

//controller to switch
#define IMPORTANT_QUEUE_ID 0
#define LLDP_QUEUE_ID 0
#define FLOW_MOD_QUEUE_1_ID 1
#define PO_QUEUE_1_ID 2
#define FLOW_MOD_QUEUE_2_ID 7
#define PO_QUEUE_2_ID 8
#define MULTI_PART_REQUEST_QUEUE_ID 0
#define LEAST_QUEUE_ID 9

#define LLDP_TYPE 0x88cc
#define ARP 0x806

class Queue {
    pthread_mutex_t queue_mutex;
    //std::queue<OFP_Msg*> msg_queue;
    std::vector<OFP_Msg*> msg_queue;
    //uint32_t interval_ms;
    timeval last_fetch;
    int32_t size;
public:
    double_t interval_ms;
    Queue(double_t interval_ms) {
        this->interval_ms = interval_ms;
        gettimeofday(&last_fetch, NULL);
    }
    int32_t getSize() {
        pthread_mutex_lock(&this->queue_mutex);
        int32_t temp = this->size;
        pthread_mutex_unlock(&this->queue_mutex);
        return temp;
    }
    int64_t putMsg(OFP_Msg* msg) {
        pthread_mutex_lock(&this->queue_mutex);

        this->msg_queue.push_back(msg);
        int64_t index = this->msg_queue.size() - 1;
        pthread_mutex_unlock(&this->queue_mutex);
        return index;
    }
    bool msgNeedNoTime(OFP_Msg* msg) {
        bool need_no_time = (!(msg->header->type == rofl::openflow::OFPT_FLOW_MOD &&
          msg->identity != 0)
        &&
        !(msg->header->type == rofl::openflow::OFPT_STATS_REQUEST &&
          msg->identity != 0)) || (msg->header->type == rofl::openflow::OFPT_PACKET_OUT);
        return need_no_time;

    }

    int32_t decMsgTime() {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
//        if(num > 0) {
//            fprintf(stderr, "\n\n dec time %u \n\n", num);
//        }
        for(uint64_t i = 0; i < num; ++i) {
            OFP_Msg* msg = this->msg_queue[i];
            msg->max_wait_time --;
            if(msg->in_process) {
                msg->process_time --;
                if(msg->process_time < 0) {
                    if (this->msgNeedNoTime(msg)) {
                        msg->finished = 1;
                    } else {
//                        fprintf(stderr, "Read(PACKET_TYPE):%d  cookies:%lu \n",
//                                this->msg_queue[i]->header->type, this->msg_queue[i]->identity);
                    }
                }
              //  fprintf(stderr, "\n\n need time %d \n\n", this->msg_queue[i]->process_time);
            }
        }
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }

    //every call mark one only
    int32_t markFinished(uint64_t identify) {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
        if(num > 0) {
           // fprintf(stderr, "\n\nTry Find Identify %lu \n\n", identify);
        }
        for(uint64_t i = 0; i < num; ++i) {
            if( this->msg_queue[i]->in_process
               && !this->msg_queue[i]->finished
               && this->msg_queue[i]->identity == identify) {
                this->msg_queue[i]->finished = 1;
                gettimeofday(&this->msg_queue[i]->end, NULL);
                //fprintf(stderr, "\n\nHas Find Identify:%lu  \n\n", identify);
                pthread_mutex_unlock(&this->queue_mutex);
                return 1;
                fprintf(stderr, "\n\n need time %d \n\n", this->msg_queue[i]->process_time);
            }
            else if(this->msg_queue[i]->header->type == rofl::openflow::OFPT_FLOW_MOD &&
                    this->msg_queue[i]->identity != 0) {
              //          fprintf(stderr, "Didn find(PACKET_TYPE):%d  cookies:%lu \n",
              //  this->msg_queue[i]->header->type, this->msg_queue[i]->identity);
            }
        }
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }

    int32_t markFinished(uint64_t identify, int64_t pos) {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
        if(pos > num || pos < 0) {
            pthread_mutex_unlock(&this->queue_mutex);
            return 0;
        }
        if(num > 0) {
          //  fprintf(stderr, "\n\nTry Find Identify %lu \n\n", identify);
        }
        OFP_Msg* msg = this->msg_queue[pos];
        if( msg->in_process
            && !msg->finished
            && msg->identity == identify) {
            msg->finished = 1;
            gettimeofday(&msg->end, NULL);
           // fprintf(stderr, "\n\nHas Find Identify:%lu  \n\n", identify);
            pthread_mutex_unlock(&this->queue_mutex);
            return 1;
        }
        else if(msg->header->type == rofl::openflow::OFPT_FLOW_MOD &&
                msg->identity != 0) {
          //            fprintf(stderr, "Didn find(PACKET_TYPE):%d  cookies:%lu \n",
          //  msg->header->type, msg->identity);
        }
        pthread_mutex_unlock(&this->queue_mutex);
        return 0;
    }

    //pop and get number of finished msgs
    int32_t cleanFinishMsg(FILE* mark) {
        pthread_mutex_lock(&this->queue_mutex);
        uint64_t num = this->msg_queue.size();
        int32_t count = 0;
        for(uint64_t i = 0,index = 0; i < num; ++i,++index) {
            //fprintf(stderr, "\n\n packet need time %d in process %d \n\n", this->msg_queue[index]->process_time, this->msg_queue[index]->in_process);

            if((this->msg_queue[index]->in_process) && (this->msg_queue[index]->finished == 1)) {
                double s = this->msg_queue[index]->end.tv_sec - this->msg_queue[index]->recv.tv_sec;
                double us = this->msg_queue[index]->end.tv_usec - this->msg_queue[index]->recv.tv_usec;
                double s2 = this->msg_queue[index]->send.tv_sec - this->msg_queue[index]->recv.tv_sec;
                double us2 = this->msg_queue[index]->send.tv_usec - this->msg_queue[index]->recv.tv_usec;

                double ms = s * 1000.0 + us / 1000.0;
                double ms2 = s2 * 1000.0 + us2 / 1000.0;
                if(this->msg_queue[index]->uepid.uid != 0) {
                    if(this->msg_queue[index]->uepid.uid == 2 || this->msg_queue[index]->uepid.uid == 4) {
                        char temp[300];
//                        size_t  n = sprintf(temp, "UEP:[%02X, %02X] Identify:%lu Used Time:%lf ms Send Time:%lf ms\n",
//                                this->msg_queue[index]->uepid.eid,
//                                this->msg_queue[index]->uepid.uid, this->msg_queue[index]->identity, ms, ms2);
                        fprintf(stderr,"UEP:[%02X, %02X] Msg_Type:%s Identify:%lu Used Time:%lf ms Send Time:%lf ms\n",
                                this->msg_queue[index]->uepid.eid,
                                this->msg_queue[index]->uepid.uid,
                                getOFPMsgType(this->msg_queue[index]->ofp_type).c_str(),
                                this->msg_queue[index]->identity, ms, ms2);

                        fprintf(mark,"UEP:[%02X, %02X] Msg_Type:%s Identify:%lu Used Time:%lf ms Send Time:%lf ms\n",
                                this->msg_queue[index]->uepid.eid,
                                this->msg_queue[index]->uepid.uid,
                                getOFPMsgType(this->msg_queue[index]->ofp_type).c_str(),
                                this->msg_queue[index]->identity, ms, ms2);
                        //fflush(mark);
                        //write(mark->_fileno, temp, n);
                    }
                    //fflush(mark);
                   // fprintf(stderr, "Resp Identify:%lu Used Time:%lf ms\n", this->msg_queue[index]->identity, ms);
                }

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

        timeval now;
        gettimeofday(&now, NULL);
        double s = now.tv_sec - last_fetch.tv_sec;
        double us = now.tv_usec - last_fetch.tv_usec;
        double ms = s * 1000.0 + us / 1000.0;
        if(ms > interval_ms) {
            last_fetch.tv_sec = now.tv_sec;
            last_fetch.tv_usec = now.tv_usec;

        } else{
           // fprintf(stderr, "no in time ! %lf %lf \n",ms, interval_ms);
            pthread_mutex_unlock(&this->queue_mutex);
            return msg;
        }

        uint64_t num = this->msg_queue.size();
        for(uint64_t i = 0; i < num; ++i) {
            if(!this->msg_queue[i]->in_process && this->msg_queue[i]->read_to_be_sent) {
                if(msg == NULL)  {
                    msg = this->msg_queue[i];
                    continue;
                }
                else if(lessPriority((*msg) , *(msg_queue[i]))){
                    msg = this->msg_queue[i];
                }
            }
            if(msg != NULL && (msg->uepid.uid == 2 ||msg->uepid.uid == 4))break;
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

struct UEP_Msg{
    Split::UEPID uepid;
    uint16_t window = 0;
    uint16_t number = 1;

}__attribute__((packed));

struct Policy{
    uint16_t oxm_class;
    uint8_t oxm_field_id;//TLV TYPE
    //uint8_t oxm_has_mask;
    uint8_t length;//TLV L
    uint8_t* value;//TLV value

    struct policy_id{
        uint8_t app_id;
        uint8_t user_id;
        bool operator < (const policy_id& id) const {
            return app_id < id.app_id || (app_id == id.app_id && user_id < id.user_id);
        }
    }p_id;
    uint8_t priority;

    Policy(uint8_t length, uint8_t* value) {
        this->value = new uint8_t[length];
        this->length = length;
        for(int i = 0; i < length; ++i) {
            this->value[i] = value[i];
        }
    }
    ~Policy() {
        delete value;
    }
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
    int16_t server_port;
    std::map<Policy::policy_id , Policy*> policies;
    PriorityManager *priorityManager;
    pthread_mutex_t policy_mutex = PTHREAD_MUTEX_INITIALIZER;
public:
    int32_t setupConf(uint16_t server_port);
    void initListenSocket() {
        this->config_fd = Socket(AF_INET, SOCK_DGRAM, 0);
        this->server.sin_addr.s_addr = INADDR_ANY;
        this->server.sin_port = htons(server_port);
        this->server.sin_family = AF_INET;
        socklen_t len = sizeof(this->server);
        int on = 1;
        SetSocket(this->config_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        Bind_Socket(this->config_fd, (SA*)&this->server, len);
    }
    void initPriorityManager() {
        this->priorityManager = new PriorityManager(5, 20);
    }
    int32_t listenRequest();
    std::map<Policy::policy_id , Policy*>* getAllPolices() {
        return &(this->policies);
    }
    void updatePolicy(Policy::policy_id pid,  uint8_t* value) {

    }
    bool hasPolicy(Policy::policy_id pid) {
        std::map<Policy::policy_id , Policy*>::iterator iter;
        iter = this->policies.find(pid);
        if(iter != this->policies.end()) return 1;
        return 0;
    }
    int addPolicy(Policy::policy_id pid, Policy* policy) {
        this->policies.insert(std::pair<Policy::policy_id , Policy*>(pid, policy));
    }
    PriorityManager* getPriorityManager() { return this->priorityManager; }

    Policy* getPolicy(Policy::policy_id pid) {
        std::map<Policy::policy_id , Policy*>::iterator iter;
        iter = this->policies.find(pid);
        if(iter != this->policies.end()) return iter->second;
        return NULL;
    }
};

struct MsgPos{
    int8_t qid;
    int64_t pid;
    MsgPos() {
        qid = -1;
        pid = -1;
    }
    MsgPos(int8_t qid, int64_t pid) {
        this->qid = qid;
        this->pid = pid;
    }
    bool validPos() {
        return qid != -1 && pid != -1;
    }
};

class Schedule {
    std::vector<Queue*> queues;
    std::vector<Queue*> *other_queues;
    std::priority_queue<OFP_Msg*> send_list;
    std::map<uint64_t , MsgPos> pos_map;
    pthread_mutex_t policy_mutex;
    PolicyConfig* conf;
    int32_t queue_num;
    FILE* mark_result_fd;

    char resp_pipe[50];
    char name[30];
    char resp_record_name[50];
    bool get_dp_id;
    bool is_listen_resp;
    char dp_id;

   // int32_t fd;
public:
    char* filter_buf;
    void setDpid(char dp_id) {
        this->dp_id = dp_id;
        uint32_t lens = strlen(this->resp_pipe);
        this->resp_pipe[lens] = this->dp_id;
        this->resp_pipe[lens+1] = 0;
    }
    bool isSWToControl() {
        return !(strcmp(this->name, "SERVER") == 0);
    }
    void setGet_dp_id(bool get) { this->get_dp_id = get; }
    Schedule* other;

    Schedule(char* name, char* pipe_name, uint16_t port) {
        policy_mutex = PTHREAD_MUTEX_INITIALIZER;
        this->filter_buf = new char[600];
        this->get_dp_id = this->is_listen_resp = 0;
        this->dp_id = 0;

        strcpy(this->name, name);
        strcpy(this->resp_pipe, pipe_name);

        int n = sprintf(this->resp_record_name, "%s_%d",pipe_name, port);

        /* Open Record Process Time File */
        mark_result_fd = fopen(this->resp_record_name, "a");
        if(mark_result_fd < 0) {
            fprintf(stderr, "Open Resp Record:%s, Error:%s\n", this->resp_record_name, strerror(errno));
        }

        /* Init Multi-Queue */
        this->queue_num = 10;
        for(int i = 0; i < this->queue_num; ++i) {
            Queue *q;
            if(i < 5) {
                q = new Queue(-1);
            }
            else {
                q = new Queue(i * 20);
            }
            queues.push_back(q);
        }
    }

    int32_t putMessage(char* msg, int32_t len, int32_t fd);
    int32_t run();
    OFP_Msg_Arg getOFPMsgArg(char* msg);
    std::vector<Queue*> *getOtherQueues(){
        return this->other_queues;
    }
    PolicyConfig* getConf() { return this->conf; }
    std::vector<Queue*> *getQueues(){
        return &this->queues;
    }
    void setConf(PolicyConfig* conf) {
        this->conf = conf;
    }
    void setOtherQueue(std::vector<Queue*> *other_queues) {
        this->other_queues = other_queues;
    }
    ~Schedule() {
        fclose(mark_result_fd);
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
