//
// Created by dalaoshe on 17-7-16.
//

#include "schedule.h"
using namespace rofl;
int32_t Schedule::putMessage(char* msg, int32_t len, int32_t fd) {
    int32_t priority = this->getPriority(msg);
    int32_t qid = this->getQueueId(msg);
    OFP_Msg* ofp_msg = new OFP_Msg(msg, len, fd, priority);
    ofp_msg->max_wait_time = this->getMaxWaitTime(msg);
    ofp_msg->process_time = this->getProcessTime(msg);
    switch(qid) {
        case PI_QUEUE_ID: {//packet_in
          //  fprintf(stderr, "Client PUT PACKET_IN MSG\n");
            //pi_queue.putMsg(ofp_msg);
            break;
        }
        case MSG_QUEUE_ID: {
           // fprintf(stderr, "Client PUT OF MSG\n");
            //msg_queue.putMsg(ofp_msg);
            break;
        }
        default: {//other
           // fprintf(stderr, "Client PUT OTHER MSG\n");
            //msg_queue.putMsg(ofp_msg);
            break;
        }
    }
    this->queues[qid]->putMsg(ofp_msg);
}

struct PipeListenArg {
    char* name;
    std::vector<Queue*>* listenQueue;
};
void* Run_listen_resp(void* argv) {
    PipeListenArg* arg = (PipeListenArg*)argv;
    std::vector<Queue*>* queues = arg->listenQueue;

    fprintf(stderr, "\n\nOpen Listen Pipe\n\n");
    int fd = mkfifo(arg->name, O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "\n\nCreate fifo pipe Error:%s\n\n", strerror(errno));
        fd = open(arg->name, O_RDONLY | O_NONBLOCK);
        if(fd < 0) {
            fprintf(stderr, "\n\nOpen fifo pipe Error:%s\n\n", strerror(errno));
        }
    }
    fprintf(stderr, "\n\nOpen fifo pipe SUCCESS\n\n");

    char buf[1024];
    int n;
    while (1) {
        n = read(fd, buf, 1024);
        if(n > 0) {
            fprintf(stderr, "\n\nRead %d byte from pipe, content:%s\n\n", n, buf);
            int q_num = queues->size();
            uint8_t identify = 0;
            for(int i = 0; i < q_num; ++i) {
                Queue* q = (*queues)[i];
                int marked = q->markFinished(identify);
                if(marked )break;
            }
        }
    }
}

int32_t Schedule::run() {

    //start piplisten
    PipeListenArg* pipeArg = new PipeListenArg();
    pipeArg->name = this->resp_pipe;
    pipeArg->listenQueue = &this->queues;
    pthread_t pipe_t;
//    strcpy(pipeArg->name, pipe_name);
    pthread_create(&pipe_t, NULL, &Run_listen_resp, (void*)pipeArg);

    int32_t  times = 0;
    int32_t max_in_process = 1;
    int on = 1, off = 0;
    while (1) {
        //update queue
        int32_t clean_num = 0;
        for(int i = 0; i < this->queue_num; ++i) {
            Queue *q = this->queues[i];
            q->decMsgTime();
            clean_num += q->cleanFinishMsg();
        }
        max_in_process += clean_num;

        while(max_in_process > 0) {
            OFP_Msg* msg = NULL;
            bool has_msg = false;
            for(int i = 0; i < this->queue_num; ++i) {
                Queue* q = this->queues[i];
                msg = q->fetchMsg();
                if(msg == NULL) continue;
                //send msg
                SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&on, sizeof(on));
                Writev_nByte(msg->fd, msg->buf, msg->len);
                SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&off, sizeof(off));
                //mark
                msg->in_process = 1;
                has_msg = true;
                //print info
                struct openflow::ofp_header *header =
                        (struct openflow::ofp_header *)(msg->buf);
                if(!(header->type == openflow::OFPT_PACKET_IN)) {
                   // fprintf(stderr, "%s:Read(OTHER_MSG,TYPE:%d) %d byte SEND to server_fd %d \n",this->name, header->type, msg->len, msg->fd);
                }
                else if(header->type == openflow::OFPT_PACKET_IN) {
                   // fprintf(stderr, "%s:Read(PACKET_IN) %d byte SEND to server_fd %d \n",this->name, msg->len, msg->fd);
                }

                break;
            }
            //no msg need process
            if(!has_msg) {
              //  fprintf(stderr, "\n\n get not msg , PI_QUEUE_SIZE:%d MSG_QUEUE_SIZE:%d \n\n", pi_queue.getSize(), msg_queue.getSize());
                break;
            }
            else {
                max_in_process--;
            }
        }
        //usleep(100);
//        times++;
//        if(pi_queue.getSize() +  msg_queue.getSize() >= 2) {
//            fprintf(stderr, "\n\n iter:%d , PI_QUEUE_SIZE:%d MSG_QUEUE_SIZE:%d MAX_SIZE:%d\n\n", times, pi_queue.getSize(), msg_queue.getSize(), max_in_process);
//        }
    }
}

int32_t Schedule::addPolicy(Policy *policy) {

}


//todo
int32_t Schedule::getQueueId(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    if(header->type == openflow::OFPT_PACKET_IN) {
        return PI_QUEUE_ID;
    }
    else {
        return MSG_QUEUE_ID;
    }
}

int32_t Schedule::getPriority(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;
//todo
    return 0;
}

void*  schedule_thread(void* argv)  {
    ScheduleArg* scheduleArg = (ScheduleArg*)argv;
    Schedule* schedule = scheduleArg->schedule;
    schedule->setConf(scheduleArg->config);
    schedule->run();
}

int32_t Schedule::getProcessTime(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    if(header->type == openflow::OFPT_PACKET_IN) {
        return 1;
    }
    else {
        return 1;
    }
}

int32_t Schedule::getMaxWaitTime(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    if(header->type == openflow::OFPT_PACKET_IN) {
        return 1;
    }
    else {
        return 1;
    }
}

int32_t PolicyConfig::setupConf() {
    this->config_fd = Socket(AF_INET, SOCK_DGRAM, 0);
    this->server.sin_addr.s_addr = INADDR_ANY;
    this->server.sin_port = htons(CONFIG_PORT);
    this->server.sin_family = AF_INET;
    socklen_t len = sizeof(this->server);
    int on = 1;
    SetSocket(this->config_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    SetSocket(this->config_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    Bind_Socket(this->config_fd, (SA*)&this->server, len);
}

int32_t PolicyConfig::listenRequest() {
    PolicyMsg msg, response;
    size_t max_len = sizeof(msg);
    memset(&msg, 0, sizeof(msg));
    sockaddr_in client;
    socklen_t len = sizeof(client);
    int flag = 0;
    while(1) {
        ssize_t n = recvfrom(this->config_fd, (char*)&msg, max_len, flag, (SA*)&client, &len);
        if(n == 0) {
            fprintf(stderr, "RECEV 0 BYTE\n");
            continue;
        }
        if(n < 0) {
            fprintf(stderr, "RECEV ERROR\n");
            continue;
        }
        else {
            if(msg.type == POLICY_ADD) {
                char *buf = msg.data;

                for (int i = 0; i < msg.policy_num; ++i) {
                    Policy *temp = (Policy *) buf;
                    int32_t policy_len = temp->h.match_len + sizeof(temp->h);
                    char *data = new char[policy_len];
                    for (int i = 0; i < policy_len; ++i) {
                        data[i] = buf[i];
                    }
                    Policy *policy = (Policy *) data;
                    this->policies.push_back(policy);
                }

                response.policy_num = 0;
                response.type = POLICY_RESPONSE;
                response.byte_len = sizeof(PolicyMsg);
                response.data[0] = 0;
                response.data[1] = 0;
                response.data[2] = 0;
                response.data[3] = 0;
                ssize_t r = sendto(this->config_fd, (char*)&response, response.byte_len, flag, (SA*)&client, len);
                if(r <= 0) {
                    fprintf(stderr, "SEND ERROR\n");
                }
            }
        }
    }
}

bool lessPriority (const OFP_Msg &t1, const OFP_Msg &t2) {
    if(t1.max_wait_time <= 0 && t2.max_wait_time > 0) {
        return false;
    }
    else if(t1.max_wait_time <= 0 && t2.max_wait_time <= 0) {
        return t1.max_wait_time > t2.max_wait_time;
    }
    else {
        return t1.priority < t2.priority;
    }
}
