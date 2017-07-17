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
            fprintf(stderr, "Client PUT PACKET_IN MSG\n");
            pi_queue.putMsg(ofp_msg);
            break;
        }
        case MSG_QUEUE_ID: {
            fprintf(stderr, "Client PUT OF MSG\n");
            msg_queue.putMsg(ofp_msg);
            break;
        }
        default: {//other
            fprintf(stderr, "Client PUT OTHER MSG\n");
            msg_queue.putMsg(ofp_msg);
            break;
        }
    }
}

int32_t Schedule::run() {
    int32_t  times = 0;
    int on = 1, off = 0;
    while (1) {
        OFP_Msg* msg = msg_queue.fetchMsg();
        if(msg != NULL) {
            struct openflow::ofp_header *header =
                    (struct openflow::ofp_header *)(msg->buf);
            if(!(header->type == openflow::OFPT_PACKET_IN)) {
                fprintf(stderr, "%s:Read(OTHER_MSG,TYPE:%d) %d byte SEND to server_fd %d \n",this->name, header->type, msg->len, msg->fd);
                SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&on, sizeof(on));
                Writev_nByte(msg->fd, msg->buf, msg->len);
                SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&off, sizeof(off));
            }
            delete msg;
        }
        else {
            msg = pi_queue.fetchMsg();
            if(msg != NULL) {
                struct openflow::ofp_header *header =
                        (struct openflow::ofp_header *)(msg->buf);
                if(header->type == openflow::OFPT_FLOW_MOD || header->type == openflow::OFPT_PACKET_IN) {
                    fprintf(stderr, "%s:Read(PACKET_IN) %d byte SEND to server_fd %d \n",this->name, msg->len, msg->fd);
                    SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&on, sizeof(on));
                    Writev_nByte(msg->fd, msg->buf, msg->len);
                    SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK,(char*)&off, sizeof(off));
                }
                delete msg;
            }
        }
//        times++;
//        if(times % 1000 == 0) {
//            fprintf(stderr, "\n\n iter:%d , PI_QUEUE_SIZE:%d MSG_QUEUE_SIZE:%d \n\n", times, pi_queue.getSize(), msg_queue.getSize());
//        }
    }
}

int32_t Schedule::run2() {
    int32_t  times = 0;
    int32_t max_in_process = 1;
    int on = 1, off = 0;
    while (1) {
        this->msg_queue.decMsgTime();
        this->pi_queue.decMsgTime();
        int32_t clean_num = 0;
        clean_num += this->msg_queue.cleanFinishMsg();
        clean_num += this->pi_queue.cleanFinishMsg();
        if(clean_num > 0) {
            fprintf(stderr, "\n\n\n CLEAN:%d \n\n\n", clean_num);
        }
        max_in_process += clean_num;
        while(max_in_process > 0) {
            OFP_Msg* msg = msg_queue.fetchMsg();
            if(msg != NULL) {
                struct openflow::ofp_header *header =
                        (struct openflow::ofp_header *)(msg->buf);
                if(!(header->type == openflow::OFPT_PACKET_IN)) {
                    fprintf(stderr, "%s:Read(OTHER_MSG,TYPE:%d) %d byte SEND to server_fd %d \n",this->name, header->type, msg->len, msg->fd);
                    SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&on, sizeof(on));
                    msg->in_process = 1;
                    Writev_nByte(msg->fd, msg->buf, msg->len);
                    SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&off, sizeof(off));
                    max_in_process --;
                }
                //delete msg;
            }
            else {
                msg = pi_queue.fetchMsg();
                if(msg != NULL) {
                    struct openflow::ofp_header *header =
                            (struct openflow::ofp_header *)(msg->buf);
                    if(header->type == openflow::OFPT_PACKET_IN) {
                        fprintf(stderr, "%s:Read(PACKET_IN) %d byte SEND to server_fd %d \n",this->name, msg->len, msg->fd);
                        SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&on, sizeof(on));
                        msg->in_process = 1;
                        Writev_nByte(msg->fd, msg->buf, msg->len);
                        SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK,(char*)&off, sizeof(off));
                        max_in_process --;
                    }
                    //delete msg;
                }
            }
            //no msg need process
            if(msg == NULL) {
              //  fprintf(stderr, "\n\n get not msg , PI_QUEUE_SIZE:%d MSG_QUEUE_SIZE:%d \n\n", pi_queue.getSize(), msg_queue.getSize());
                break;
            }
        }

//        times++;
        if(pi_queue.getSize() +  msg_queue.getSize() >= 2) {
        //    fprintf(stderr, "\n\n iter:%d , PI_QUEUE_SIZE:%d MSG_QUEUE_SIZE:%d MAX_SIZE:%d\n\n", times, pi_queue.getSize(), msg_queue.getSize(), max_in_process);
        }
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
    schedule->run2();
}

int32_t Schedule::getProcessTime(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    if(header->type == openflow::OFPT_PACKET_IN) {
        return 30;
    }
    else {
        return 10;
    }
}

int32_t Schedule::getMaxWaitTime(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    if(header->type == openflow::OFPT_PACKET_IN) {
        return 30;
    }
    else {
        return 10;
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
