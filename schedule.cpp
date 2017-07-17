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
            //pi_queue.putMsg(ofp_msg);
            break;
        }
        case MSG_QUEUE_ID: {
            fprintf(stderr, "Client PUT OF MSG\n");
            //msg_queue.putMsg(ofp_msg);
            break;
        }
        default: {//other
            fprintf(stderr, "Client PUT OTHER MSG\n");
            //msg_queue.putMsg(ofp_msg);
            break;
        }
    }
    this->queues[qid]->putMsg(ofp_msg);
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
                    fprintf(stderr, "%s:Read(OTHER_MSG,TYPE:%d) %d byte SEND to server_fd %d \n",this->name, header->type, msg->len, msg->fd);
                }
                else if(header->type == openflow::OFPT_PACKET_IN) {
                    fprintf(stderr, "%s:Read(PACKET_IN) %d byte SEND to server_fd %d \n",this->name, msg->len, msg->fd);
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
        usleep(100);
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
    schedule->run2();
}

int32_t Schedule::getProcessTime(char *msg) {
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    if(header->type == openflow::OFPT_PACKET_IN) {
        return 10;
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
        return 10;
    }
    else {
        return 50;
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
