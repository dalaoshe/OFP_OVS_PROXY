//
// Created by dalaoshe on 17-7-16.
//

#include "schedule.h"
using namespace rofl;
int32_t Schedule::putMessage(char* msg, int32_t len, int32_t fd) {
    int32_t priority = this->getPriority(msg);
    //int32_t qid = this->getQueueId(msg);
    OFP_Msg_Arg arg = this->getOFPMsgArg(msg);
    OFP_Msg* ofp_msg = new OFP_Msg(msg, arg.len, fd, priority);

    ofp_msg->identity = arg.identify;
    ofp_msg->max_wait_time = this->getMaxWaitTime(msg);
    ofp_msg->process_time = this->getProcessTime(msg);
    switch(arg.qid) {
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
    this->queues[arg.qid]->putMsg(ofp_msg);
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


            int q_num = queues->size();
            uint64_t identify = *((uint64_t*)buf);
           // fprintf(stderr, "\n\nRead %d byte from pipe, content:%lu\n\n", n, identify);
            for(int i = 0; i < q_num; ++i) {
                Queue* q = (*queues)[i];
                int marked = q->markFinished(identify);
                if(marked )break;
            }
        }
    }
}

int32_t Schedule::run() {

    /* Start Pipelisten Thread To do Resp */
    PipeListenArg* pipeArg = new PipeListenArg();
    pipeArg->name = this->resp_pipe;
    pipeArg->listenQueue = &this->queues;
    pthread_t pipe_t;
    pthread_create(&pipe_t, NULL, &Run_listen_resp, (void*)pipeArg);

    int32_t max_in_process = 20;
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
            for(int i = 0; i < this->queue_num; ++i) {
                Queue* q = this->queues[i];
                msg = q->fetchMsg();
                if(msg == NULL) continue;
                //send msg
                //SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&on, sizeof(on));
                msg->in_process = 1;
                Writev_nByte(msg->fd, msg->buf, msg->len);
                //SetSocket(msg->fd, IPPROTO_TCP, TCP_CORK, (char*)&off, sizeof(off));
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
            if(msg == NULL) {
                break;
            }
            else {
                max_in_process--;
            }
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


uint32_t getMatchLenth(char* match) {
    uint16_t total_length = ntohs(*(uint16_t*)(match + 2));
    fprintf(stderr, "%02X %02X, total_len %d",(*((char*)(match + 2))), *((char*)(match+3)), total_length);
    size_t pad = (0x7 & total_length);
    /* append padding if not a multiple of 8 */
    if (pad) {
        total_length += 8 - pad;
    }
    return total_length;
}

void pack_mathch_except_type(char* match_msg, uint16_t ofpxmc_id, uint8_t field_id,
                             char* buf,uint16_t* new_len, uint16_t* except_len) {
    uint16_t new_match_length = 0;
    uint16_t old_match_length = ntohs(*(uint16_t*)(match_msg + 2));

//                size_t pad = (0x7 & total_length);
//                /* append padding if not a multiple of 8 */
//                if (pad) {
//                    total_length += 8 - pad;
//                }
    uint16_t total_length = old_match_length + 4;
    uint16_t match_item_len = 0;
    char* new_match_begin = buf;
    char* old_match_begin = match_msg;

    char *match_item = (old_match_begin)+ 4;
    char *new_match_item = (new_match_begin)+ 4;
    char *ins = (match_msg + total_length);
    int con = 0;
    while (match_item < ins - 4) {
        ofp_match_items* item = (ofp_match_items*)match_item;
        fprintf(stderr, "\n%d\n"
                "PACK CLASS:%02X\n"
                "PACK FIELD:%02X\n"
                "PACK LENGTH:%d\n"
                "PACK VALUE:",con, item->oxm_class, item->oxm_field_id,  item->length );
        char *value = (char*)&(item->value);
        for(int i = 0; i < item->length; ++i)
            fprintf(stderr,"%02X",*(value+i));
        fprintf(stderr,"\n");
        match_item += 4 + item->length;
        con += 1;

        if(ntohs(item->oxm_class) == ofpxmc_id && item->oxm_field_id == field_id) {
            fprintf(stderr, "\n%d\n"
                    "TICK CLASS:%02X\n"
                    "TICK FIELD:%02X\n"
                    "TICK LENGTH:%d\n"
                    "TICK VALUE:",con, item->oxm_class, item->oxm_field_id,  item->length );
            char *value = (char*)&(item->value);
            for(int i = 0; i < item->length; ++i)
                fprintf(stderr,"%02X",*(value+i));
            fprintf(stderr,"\n");
            match_item_len += 4 + item->length;
            continue;
        }
        /* copy */
        ofp_match_items* new_item = (ofp_match_items*)new_match_item;
        new_item->oxm_class = item->oxm_class;
        new_item->oxm_field_id = item->oxm_field_id;
        new_item->length = item->length;
        //new_item->value = item->value;
        char *new_value = (char*)&(new_item->value);
        for(int i = 0; i < new_item->length; ++i)
            *(new_value+i) = *(value+i);
        new_match_item += 4 + new_item->length;
            //fprintf(stderr,"%02X",*(value+i));

    }
    new_match_length = total_length - match_item_len;
    *(uint16_t*)buf = *(uint16_t*)match_msg;
    *(uint16_t*)(buf + 2) = htons(new_match_length - 4);
    fprintf(stderr, "old len:%u new len:%u \n", total_length, new_match_length);
    *except_len = match_item_len;
    *new_len = new_match_length;
}

//todo
OFP_Msg_Arg Schedule::getOFPMsgArg(char *msg) {
    OFP_Msg_Arg arg;
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    int32_t msg_len = be16toh(header->length);
    uint32_t xid = header->xid;

    switch (header->type) {
        case openflow::OFPT_PACKET_IN: {
            arg.qid = PI_QUEUE_ID;

            ofp_packet_in* packet_in = (ofp_packet_in*)msg;
            char* match = (char*)(&packet_in->match);


            uint16_t match_total_length = ntohs(packet_in->match.length) ;
            match_total_length += 4;
            uint16_t pad_len = 2;

            char* eth_data = match + match_total_length + pad_len;
            uint16_t *type = (uint16_t*)(eth_data + 12);
            uint16_t e_type = ntohs(*type);
            if(e_type == LLDP_TYPE) {
                arg.qid = LLDP_QUEUE_ID;
                arg.priority = 0xFF;
                arg.identify = header->xid;
                arg.max_wait_time = arg.process_time = 0;
            } else {
                arg.qid = PI_QUEUE_ID;
            }
            fprintf(stderr, "RECEIVE PI ETH_TYPE:%u match_len:%u\n", e_type, match_total_length);
            break;
        }
        case openflow::OFPT_FLOW_MOD: {
            arg.qid = FLOW_MOD_QUEUE_ID;
            break;
        }
        case openflow::OFPT_PACKET_OUT: {
            arg.qid = PO_QUEUE_ID;
            ofp_packet_out* packet_out = (ofp_packet_out*)msg;
            char* action = (char*)(&packet_out->actions);
            char* eth_data = action + ntohs(packet_out->actions_len);
            uint16_t *type = (uint16_t*)(eth_data + 12);
            uint16_t e_type = ntohs(*type);
            if(e_type == LLDP_TYPE) {
                arg.qid = LLDP_QUEUE_ID;
                arg.priority = 0xFF;
                arg.identify = header->xid;
                arg.max_wait_time = arg.process_time = 0;
            } else {
                arg.qid = PO_QUEUE_ID;
            }
            break;
        }
        case openflow::OFPT_STATS_REQUEST: {
            arg.qid = MULTI_PART_REQUEST_QUEUE_ID;
            arg.identify = ntohl(header->xid);
           // fprintf(stderr, "RECEIVE FLOW REQUEST COOKIE:%lu \n", arg.identify);
            break;
        }
        case openflow::OFPT_STATS__REPLY: {
            arg.qid = MULTI_PART_REPLY_QUEUE_ID;
            arg.identify = ntohl(header->xid);
           // fprintf(stderr, "RECEIVE FLOW REPLY COOKIE:%lu \n", arg.identify);
            //mark request
            Queue* q = (*this->other_queues)[MULTI_PART_REQUEST_QUEUE_ID];
            int marked = q->markFinished(arg.identify);

            break;
        }
        default: {
            arg.qid = MSG_QUEUE_ID;
        }
    }

    if(header->type == openflow::OFPT_FLOW_MOD) {
        switch (header->version) {
            case 4: {
                struct ofp13_flow_mod *hdr = (struct ofp13_flow_mod *) (msg + sizeof(struct openflow::ofp_header));
                arg.identify = hdr->cookie;
              //  fprintf(stderr, "RECEIVE FLOW MOD COOKIE:%lu \n", arg.identify);

                uint16_t total_length = ntohs(*(uint16_t*)(((char*)&(hdr->match)) + 2));
                total_length += 4;
                fprintf(stderr, "\nold match len:%u \n", total_length);

                char *match_item = ((char*)&(hdr->match))+ 4;
                bool has_inst = ntohs(header->length) - sizeof(struct openflow::ofp_header) - sizeof(struct ofp13_flow_mod) - total_length >= 2;
                char *ins = (((char *) &(hdr->match)) + total_length);
                int con  = 0;
                while (match_item < ins - 4) {
                    ofp_match_items* item = (ofp_match_items*)match_item;
                    fprintf(stderr, "\n%d\n"
                            "CLASS:%02X\n"
                            "FIELD:%02X\n"
                            "LENGTH:%d\n"
                            "VALUE:",con, item->oxm_class, item->oxm_field_id,  item->length );
                    char *value = (char*)&(item->value);
                    for(int i = 0; i < item->length; ++i)
                    fprintf(stderr,"%02X",*(value+i));
                    fprintf(stderr,"\n");
                    con += 1;
                    match_item += 4 + item->length;
                }

                char* buf = new char[total_length];
                memset(buf, 0, total_length);
                uint16_t new_len = 0, except_len = 0;
                pack_mathch_except_type(((char*)&(hdr->match)), 0, 0, buf, &new_len, &except_len);
                header->length = htons(ntohs(header->length) - except_len);
                /* copy new len */
                for(int i = 0; i < new_len; ++i) {
                    *(((char*)&(hdr->match)) + i) = buf[i];
                }
                /* copy instruction */
                if(has_inst) {
                    uint16_t inst_len = ntohs(*((uint16_t*)(ins+2)));
                    for (int i = 0; i < inst_len; ++i) {
                        *(((char*)&(hdr->match)) + i + new_len) = ins[i];
                    }
                    fprintf(stderr, "\n\ninst len:%u \n\n", inst_len);
                }

                /* after copy */
                total_length = ntohs(*(uint16_t*)(buf + 2));


                total_length += 4;
                fprintf(stderr, "\nnew match len:%u \n", total_length);
                match_item = buf+ 4;
                ins = (buf + total_length);
                con = 0;
                while (match_item < ins - 4) {
                    ofp_match_items* item = (ofp_match_items*)match_item;
                    fprintf(stderr, "\n%d\n"
                            "NEW CLASS:%02X\n"
                            "NEW FIELD:%02X\n"
                            "NEW LENGTH:%d\n"
                            "NEW VALUE:",con, item->oxm_class, item->oxm_field_id,  item->length );
                    char *value = (char*)&(item->value);
                    for(int i = 0; i < item->length; ++i)
                        fprintf(stderr,"%02X",*(value+i));
                    fprintf(stderr,"\n");
                    con += 1;
                    match_item += 4 + item->length;
                }
                fprintf(stderr, "pay load\n");
                for(int i = 0; i < ntohs(header->length); ++i) {
                    fprintf(stderr,"%02X ",*(msg+i));
                    if((i+1) % 16 == 0) fprintf(stderr, "\n");
                }
                fprintf(stderr, "\n");
                fprintf(stderr, "\n\n len:%u \n\n", ntohs(header->length));
                break;
            }
            default: {

            }
        }
    }
    arg.len = ntohs(header->length);
    return arg;
}

int32_t PolicyConfig::setupConf(uint16_t server_port) {
    this->server_port = server_port;

    this->config_fd = Socket(AF_INET, SOCK_DGRAM, 0);
    this->server.sin_addr.s_addr = INADDR_ANY;
    this->server.sin_port = htons(server_port);
    this->server.sin_family = AF_INET;

    socklen_t len = sizeof(this->server);
    int on = 1;
    SetSocket(this->config_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
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
