//
// Created by dalaoshe on 17-7-16.
//

#include "schedule.h"
using namespace rofl;
int32_t Schedule::putMessage(char* msg, int32_t len, int32_t fd) {
    OFP_Msg_Arg arg = this->getOFPMsgArg(msg);

    OFP_Msg* ofp_msg = new OFP_Msg(msg, arg.len, fd, arg.priority);

    ofp_msg->identity = arg.identify;
    ofp_msg->uepid = arg.uepid;
    ofp_msg->len = arg.len;
    ofp_msg->priority = arg.priority;
    ofp_msg->max_wait_time = arg.max_wait_time;
    ofp_msg->process_time = arg.process_time;
    int64_t pid = this->queues[arg.qid]->putMsg(ofp_msg);
    MsgPos pos = MsgPos(arg.qid, pid);
    this->pos_map.insert(std::pair<uint64_t , MsgPos>(ofp_msg->identity, pos));
    ofp_msg->read_to_be_sent = 1;
    //fprintf(stderr, "UEP:[%02X,%02X] Msg_Type:%s Priority:%lf Identify:%lu \n",arg.uepid.eid, arg.uepid.uid, getOFPMsgType(ofp_msg->ofp_type).c_str() , arg.priority, arg.identify);
}

struct PipeListenArg {
    char* name;
    std::vector<Queue*>* listenQueue;
    std::map<uint64_t , MsgPos>* pos_map;
};
void* Run_listen_resp(void* argv) {
    PipeListenArg* arg = (PipeListenArg*)argv;
    std::vector<Queue*>* queues = arg->listenQueue;
    std::map<uint64_t , MsgPos>* pos_map = arg->pos_map;
    fprintf(stderr, "\n\nOpen Listen Pipe :%s\n\n", arg->name);
    int fd = mkfifo(arg->name, O_RDONLY);
    if(fd < 0) {
        fprintf(stderr, "\n\nCreate fifo pipe Error:%s\n\n", strerror(errno));
        fd = open(arg->name, O_RDONLY | O_NONBLOCK);
        //fd = open(arg->name, O_RDONLY);
        if(fd < 0) {
            fprintf(stderr, "\n\nOpen fifo pipe Error:%s\n\n", strerror(errno));
        }
    }
    fprintf(stderr, "\n\nOpen fifo pipe SUCCESS\n\n");

    char buf[1024];
    int n = 0;
    uint64_t total_resp = 0;
    while (1) {
        n = read(fd, buf, sizeof(uint64_t));
        while(n > 0) {

            uint64_t identify = *((uint64_t*)buf);
            //fprintf(stderr, "\n\nRead %d byte from pipe, content:%lu\n\n", n, identify);

            //MsgPos pos = (*pos_map)[identify];
            //fprintf(stderr, "\n\nRead %d byte from pipe, content:%lu Pos:[%d,%ld]\n\n", n, identify, pos.qid, pos.pid);
            int marked = 0 ;//(*queues)[pos.qid] ->markFinished(identify, pos.pid);

            if(!marked) {
                int q_num = queues->size();
                for(int i = 0; i < q_num; ++i) {
                    Queue* q = (*queues)[i];
                    marked = q->markFinished(identify);
                    if(marked)break;
                }
            }
//            if(!marked) {
//                fprintf(stderr, "\n\nRead %d byte from pipe, content:%lu No Match !!!\n\n", n, identify);
//            }
//
//            if(marked) {
//                total_resp += 1;
//                if(total_resp % 1000 == 0)
//                fprintf(stderr, "\n\nTotal Resp %lu \n\n", total_resp);
//            }

            n = read(fd, buf, sizeof(uint64_t));
        }
        //fprintf(stderr, "READ PIPE\n");
        /* Avoid No Need CPU */
        usleep(10000);
    }
}

int32_t Schedule::run() {

    /* Start Pipelisten Thread To do Resp */
    PipeListenArg* pipeArg = new PipeListenArg();

    pipeArg->listenQueue = &this->queues;
    pipeArg->pos_map = &this->pos_map;
    pthread_t pipe_t;

    int32_t max_in_process = 10;

    while (1) {

        /* Start up Listen Response Thread */
        if(this->get_dp_id && !this->is_listen_resp) {
            pipeArg->name = this->resp_pipe;
            pthread_create(&pipe_t, NULL, &Run_listen_resp, (void*)pipeArg);
            this->is_listen_resp = 1;
        }

        /* Update Queue */
        int32_t clean_num = 0;
        if(max_in_process < 2 || this->isSWToControl()) {
            for (int i = 0; i < this->queue_num; ++i) {
                Queue *q = this->queues[i];
                clean_num += q->cleanFinishMsg(this->mark_result_fd);
            }
        }
        max_in_process += clean_num;
//        if(clean_num > 0)
//            fprintf(stderr, "\n Clean UP %d Max_in_Process %d\n", clean_num, max_in_process);


        while(max_in_process > 0) {
            OFP_Msg* msg = NULL;
            for(int i = 0; i < this->queue_num; ++i) {
                Queue* q = this->queues[i];
                msg = q->fetchMsg();
                if(msg == NULL) continue;

                msg->in_process = 1;
                gettimeofday(&msg->send, NULL);
                struct openflow::ofp_header *header =
                        (struct openflow::ofp_header *)(msg->buf);

//                if((header->type == openflow::OFPT_FLOW_MOD)) {
//                    if(msg->uepid.uid == 1) {
//                        msg->finished = 1;
//                        max_in_process ++;
//                        break;
//                    }
//                }


                Writev_nByte(msg->fd, msg->buf, msg->len);
                if(q->msgNeedNoTime(msg)) {
                    gettimeofday(&msg->end, NULL);
                    msg->finished = 1;
                }

                /* Debug: print info */


//                if((header->type == openflow::OFPT_FLOW_MOD)) {
////                    fprintf(stderr, "%s:Read(OTHER_MSG,TYPE:%d) %d byte SEND to server_fd %d \n",this->name, header->type, msg->len, msg->fd);
//                }
//                else if(header->type == openflow::OFPT_PACKET_IN) {
//  //                  fprintf(stderr, "%s:Read(PACKET_IN) %d byte SEND to server_fd %d \n",this->name, msg->len, msg->fd);
//                }
//                else if(!(header->type == openflow::OFPT_FLOW_MOD) && !(header->type == openflow::OFPT_PACKET_OUT)
//                        && (header->type == openflow::OFPT_ECHO_REQUEST)) {
//  //                  fprintf(stderr, "%s Send Msg_Type:%s Priority:%lf Identify:%lu Finished:%u\n",
//   //                         this->name, getOFPMsgType(header->type).c_str(), msg->priority, msg->identity, msg->finished);
//                }
//                else if(header->type == openflow::OFPT_PACKET_OUT) {
//                   // fprintf(stderr, "PACKET OUT FINISHED : %u\n", msg->finished);
//                }
//                else if(!(header->type == openflow::OFPT_FLOW_MOD)) {
//                 //   fprintf(stderr, "ELSE NOT PACKET FLOW MOD FINISHED : %u\n", msg->finished);
//                }
                break;
            }
            //no msg need process
            if(msg == NULL) {
                break;
            }
            else {
                if(!this->isSWToControl()) {
                    max_in_process--;
                }
                else
                {/* Don't Schedule Sw To Controller */
                    break;
                }
            }
        }
    }
}


void*  schedule_thread(void* argv)  {
    ScheduleArg* scheduleArg = (ScheduleArg*)argv;
    Schedule* schedule = scheduleArg->schedule;
    schedule->setConf(scheduleArg->config);
    schedule->run();
}



uint16_t getMatchLenth(char* match) {
    uint16_t total_length = ntohs(*(uint16_t*)(match + 2));
    uint16_t pad = (0x7 & total_length);
    /* append padding if not a multiple of 8 */
    if (pad) {
        total_length += 8 - pad;
    }
    return total_length;
}

UEP_Msg pack_mathch_except_type(char* match_msg,
                                uint16_t ofpxmc_id, uint8_t field_id, uint8_t value_tag,
                                char* buf, uint16_t* new_len, uint16_t* except_len,
                                PolicyConfig* config) {
    UEP_Msg uep;
    uint16_t new_match_length = 0;
    uint16_t old_match_length = ntohs(*(uint16_t*)(match_msg + 2));

    uint16_t total_length = old_match_length;

    size_t pad = (0x7 & total_length);
    /* append padding if not a multiple of 8 */
    if (pad) {
        total_length += 8 - pad;
    }



    uint16_t tick_match_item_len = 0;

    char *match_item = (match_msg)+ 4;
    char *new_match_item = (buf)+ 4;

    char *ins = (match_msg + total_length);
    int con = 0;
    while (match_item < ins - 4) {
        ofp_match_items* item = (ofp_match_items*)match_item;
        char *value = (char*)&(item->value);
        /* debug: print match item */
//        fprintf(stderr, "\n%d\n"
//                "PACK CLASS:%02X\n"
//                "PACK FIELD:%02X\n"
//                "PACK LENGTH:%d\n"
//                "PACK VALUE:",
//                con, item->oxm_class, item->oxm_field_id,  item->length );
//        for(int i = 0; i < item->length; ++i)
//            fprintf(stderr,"%02X",*(value+i));
//        fprintf(stderr,"\n");
//        con += 1;

        match_item += 4 + item->length;

        /* found special match item, do process*/
        if(ntohs(item->oxm_class) == ofpxmc_id && item->oxm_field_id == field_id && *(uint8_t*)value == value_tag) {
            /* debug: print tick match item */
            char *value = (char*)&(item->value);
//            fprintf(stderr, "\n%d\n"
//                    "TICK CLASS:%02X\n"
//                    "TICK FIELD:%02X\n"
//                    "TICK LENGTH:%d\n"
//                    "TICK VALUE:",
//                    con, item->oxm_class, item->oxm_field_id,  item->length );
//            for(int i = 0; i < item->length; ++i)
//                fprintf(stderr,"%02X",*(value+i));
//            fprintf(stderr,"\n");

            /* Update Uep */
            UEP_Msg* uep_msg = (UEP_Msg*)value;
            uep.uepid.eid = uep_msg->uepid.eid;
            uep.uepid.uid = uep_msg->uepid.uid;
            uep.window = uep_msg->window;
            uep.number = uep_msg->number;
            config->getPriorityManager()->updateSplitOfUEP(uep_msg->window, uep.uepid, uep_msg->number);
            //fprintf(stderr, "UEP:[%02X,%02X] WindowID:%u AddNumber:%u\n",uep.uepid.eid, uep.uepid.uid, uep.window, uep.number);


            tick_match_item_len += 4 + item->length;

            continue;
        }

        /* copy to buf*/
        ofp_match_items* new_item = (ofp_match_items*)new_match_item;
        new_item->oxm_class = item->oxm_class;
        new_item->oxm_field_id = item->oxm_field_id;
        new_item->length = item->length;
        char *new_value = (char*)&(new_item->value);
        for(int i = 0; i < new_item->length; ++i)
            *(new_value+i) = *(value+i);
        new_match_item += 4 + new_item->length;
    }

    /* update new match length segment */
    new_match_length = total_length - tick_match_item_len;
    *(uint16_t*)buf = *(uint16_t*)match_msg;

    uint16_t old_pad = 0;
    if(pad) {
        new_match_length -= (8 - pad);
        old_pad = (8 - pad);
    }
    *(uint16_t*)(buf + 2) = htons(new_match_length);
//    fprintf(stderr, "Old Match len:%u New Math len:%u \n", old_match_length , new_match_length);

    /* firgure new padding */
    pad = (0x7 & new_match_length);
    if (pad) {
        new_match_length += 8 - pad;
        tick_match_item_len -=  ((8 - pad) - old_pad);
    }
    /* the byte num be removed */
    *except_len = (tick_match_item_len);
    /* the new mathe total len */
    *new_len = new_match_length;

    return uep;
}

UEP_Msg copy_valid_action_to_buf(char* original_action_list, uint16_t original_action_list_len,
                              uint16_t filter_action_type, uint8_t filter_value_tag,
                              char* new_action_list_buf, uint16_t* new_action_list_len, uint16_t* kick_len,
                              PolicyConfig* config) {
    UEP_Msg uep;
    char* original_action_start = original_action_list;
    char* original_action_end = original_action_list + original_action_list_len;

    char* new_action_start = new_action_list_buf;

    uint16_t filter_lens = 0;

    int con = 1;
    while (original_action_start < original_action_end) {
        ofp_action_header* action_header = (ofp_action_header*)original_action_start;
        uint16_t action_type = action_header->type;
        uint16_t action_item_len = ntohs(action_header->len);
        char* action_item_value = (char*)&(action_header->pad);
        original_action_start += action_item_len;


        /* debug: print match item */
//        fprintf(stderr, "\n%d len:%d\n"
//                "INST PACK ACTION TYPE:%02X\n"
//                "INST PACK LENGTH:%d\n"
//                "INST PACK VALUE:",
//                con,original_action_end-original_action_start, action_type, action_item_len);
//        for(int i = 0; i < action_item_len - 4; ++i)
//            fprintf(stderr,"%02X ",*(action_item_value+i));
//        fprintf(stderr,"\n");
//        con += 1;



        /* found special match item, do process*/
        if(action_type == filter_action_type) {
            char *value = action_item_value;
            /* debug: print tick match item */
//            fprintf(stderr, "\n%d\n"
//                            "INST TICK ACTION TYPE:%02X\n"
//                            "INST TICK LENGTH:%d\n"
//                            "INST TICK VALUE:",
//                    con, action_type, action_item_len);
//            for(int i = 0; i < action_item_len - 4; ++i)
//                fprintf(stderr,"%02X",*(value+i));
//            fprintf(stderr,"\n");

            /* Update Uep */
            UEP_Msg* uep_msg = (UEP_Msg*)(value + 4);
            uep.uepid.eid = uep_msg->uepid.eid;
            uep.uepid.uid = uep_msg->uepid.uid;
            uep.window = uep_msg->window;
            uep.number = uep_msg->number;
            config->getPriorityManager()->updateSplitOfUEP(uep_msg->window, uep.uepid, uep_msg->number);
         //   fprintf(stderr, "INST TICK UEP:[%02X,%02X] WindowID:%u AddNumber:%u\n",uep.uepid.eid, uep.uepid.uid, uep.window, uep.number);

            filter_lens += action_item_len;
            continue;
        }

        /* copy to buf*/
        ofp_action_header* new_action_item = (ofp_action_header*)new_action_start;
        new_action_item->type = action_header->type;
        new_action_item->len = action_header->len;
        char *new_value = (char*)&(new_action_item->pad);
        for(int i = 0; i < action_item_len - 4; ++i)
            *(new_value+i) = *(action_item_value + i);
        new_action_start += action_item_len;
    }

    /* Update Inst Lens */
    *new_action_list_len = original_action_list_len - filter_lens;

    /* The Byte Num Be Removed */
    *kick_len = (filter_lens);
    return uep;

}

UEP_Msg pack_Inst_except_type(char* inst_msg,
                                uint16_t filter_inst_type, uint16_t filter_action_type, uint8_t value_tag,
                                char* new_action_list_buf, uint16_t* new_inst_len, uint16_t* kick_len,
                                PolicyConfig* config) {
    UEP_Msg uep;

    ofp_instruction* ofp_inst = (ofp_instruction*)inst_msg;
    uint16_t original_inst_lens = ntohs(ofp_inst->len);
    uint16_t original_action_list_lens = original_inst_lens;

    if(ofp_inst->type != filter_inst_type) {
        //fprintf(stderr,"INST TYPE %02x %02x\n",ofp_inst->type, filter_inst_type);
        return uep;
    }

    char* original_action_start = 0;
    switch (htons(ofp_inst->type)) {
        case OFPIT_GOTO_TABLE: {
            ofp_instruction_goto_table* goto_table_inst = (ofp_instruction_goto_table*)inst_msg;
            original_action_start = (char*)&(goto_table_inst->pad);
            original_action_list_lens = 0;
            break;
        }
        case OFPIT_WRITE_METADATA: {
            ofp_instruction_write_metadata* write_metadata_inst = (ofp_instruction_write_metadata*)inst_msg;
            original_action_start = (char*)&(write_metadata_inst->metadata);
            original_action_list_lens = 0;
            break;
        }
        case OFPIT_WRITE_ACTIONS:
        case OFPIT_APPLY_ACTIONS:
        case OFPIT_CLEAR_ACTIONS:{
            ofp_instruction_actions* apply_inst = (ofp_instruction_actions*)inst_msg;
            original_action_start = (char*)&(apply_inst->actions);
            original_action_list_lens -= 8;
            break;
        }

        case OFPIT_METER: {
            ofp_instruction_meter* meter_inst = (ofp_instruction_meter*)inst_msg;
            original_action_start = (char*)&(meter_inst->meter_id);
            original_action_list_lens = 0;
            break;
        }            /* Apply meter (rate limiter) */
        case OFPIT_EXPERIMENTER: {
            ofp_instruction_experimenter* experimenter_inst = (ofp_instruction_experimenter*)inst_msg;
            original_action_start = (char*)&(experimenter_inst->body);
            original_action_list_lens -= 8;
            break;
        }
        default:{

        }
    }

    char* new_action_start = new_action_list_buf;

    uint16_t new_action_list_len = 0;
    uep = copy_valid_action_to_buf(original_action_start, original_action_list_lens,
                                   filter_action_type, value_tag, new_action_start, &new_action_list_len, kick_len,
                                   config);

    /* Update Inst Lens */
    *new_inst_len = original_inst_lens - *kick_len;
    ofp_inst->len = htons(*new_inst_len);



    return uep;
}


void do_ofp_flowmod_processed(char* msg, OFP_Msg_Arg &arg, Schedule* schedule) {

    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);

    struct ofp13_flow_mod *hdr = (struct ofp13_flow_mod *) (msg + sizeof(struct openflow::ofp_header));
    if(hdr->command != openflow::OFPFC_ADD) {
        arg.identify = hdr->cookie;
        arg.qid = IMPORTANT_QUEUE_ID;
        return;
    }


    //fprintf(stderr, "RECEIVE FLOW MOD COOKIE:%lu \n", arg.identify);

    uint16_t total_length = ntohs(*(uint16_t*)(((char*)&(hdr->match)) + 2));
    size_t pad = (0x7 & total_length);
    /* append padding if not a multiple of 8 */
    if (pad) {
        total_length += 8 - pad;
    }

    /* debug: print old match content */
    //fprintf(stderr, "\nOld match len:%u \n", total_length);
    char *match_item = ((char*)&(hdr->match))+ 4;
    bool has_inst = ntohs(header->length) - sizeof(struct openflow::ofp_header) - sizeof(struct ofp13_flow_mod) - total_length >= 2;
    char *ins = (((char *) &(hdr->match)) + total_length);

    /* copy true match item to buf */
    memset(schedule->filter_buf, 0, total_length+10);
    uint16_t new_len = 0, except_len = 0;
    UEP_Msg uep = pack_mathch_except_type(((char*)&(hdr->match)), 0x8000, 0x36, 0x7F, schedule->filter_buf, &new_len, &except_len, schedule->getConf());
    arg.priority = schedule->getConf()->getPriorityManager()->getUEPPriorityOfSplitK(uep.window, uep.uepid);
    arg.uepid = uep.uepid;
    //fprintf(stderr, "Match UEP:[%02X,%02X] WindowID:%u Number:%u Priority:%lf\n",arg.uepid.eid, arg.uepid.uid, uep.window, uep.number, arg.priority );
    header->length = htons(ntohs(header->length) - except_len);
    /* copy buf match to msg*/
    for(int i = 0; i < new_len; ++i) {
        *(((char*)&(hdr->match)) + i) = schedule->filter_buf[i];
    }
    /* copy instruction to valid place*/
    if(has_inst) {
        uint16_t inst_len = ntohs(*((uint16_t*)(ins+2)));
        for (int i = 0; i < inst_len; ++i) {
            *(((char*)&(hdr->match)) + i + new_len) = ins[i];
        }
    }



    // fprintf(stderr, "\n\nFlow_Mod Inst Len:%u\n", inst_len);
    /* Pack And Filter Inst */
    if(has_inst) {
        char* inst_start = (((char*)&(hdr->match)) +  new_len);
        ofp_instruction* inst_header = (ofp_instruction*)inst_start;
        uint16_t new_inst_len = ntohs(inst_header->len), filter_len = 0;
        memset(schedule->filter_buf, 0, new_inst_len+10);
        uep = pack_Inst_except_type(inst_start,
                                            0x0400, 0x1900, 0xFF,
                                            schedule->filter_buf, &new_inst_len, &filter_len,
                                            schedule->getConf());
        arg.priority = schedule->getConf()->getPriorityManager()->getUEPPriorityOfSplitK(uep.window, uep.uepid);
        arg.uepid = uep.uepid;
        arg.identify = hdr->cookie;

        if(arg.priority > 0.5) {
            arg.qid = FLOW_MOD_QUEUE_1_ID;
        }
        else{
            arg.qid = FLOW_MOD_QUEUE_2_ID;
        }

        header->length = htons(ntohs(header->length) - filter_len);
        /* copy buf action to msg*/
        if(filter_len > 0)
            for(int i = 0; i < new_inst_len - 4 ; ++i) {
                *(inst_start + 8 + i) = schedule->filter_buf[i];
            }


        /* Debug:priority */
        //fprintf(stderr, "Flow Mod Inst UEP:[%02X,%02X] WindowID:%u Number:%u Priority:%lf\n",arg.uepid.eid, arg.uepid.uid, uep.window, uep.number, arg.priority );
        /* Debug: Print Payload */

//        fprintf(stderr, "Inst Payload New\n");
//        for(int i = 0; i < ntohs(header->length); ++i) {
//            fprintf(stderr,"%02X ",*(msg+i));
//            if((i+1) % 16 == 0) fprintf(stderr, "\n");
//        }
//        fprintf(stderr, "\n");
    }

}
void do_ofp_packetout_processed(char* msg, OFP_Msg_Arg &arg, Schedule* schedule) {
    arg.qid = IMPORTANT_QUEUE_ID;
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    ofp_packet_out* packet_out = (ofp_packet_out*)msg;

    char* action = (char*)(&packet_out->actions);
    char* original_data_satrt = action + ntohs(packet_out->actions_len);
    uint16_t original_action_list_lens = ntohs(packet_out->actions_len);
    uint16_t original_data_lens = ntohs(header->length) - original_action_list_lens - 24;
    memset(schedule->filter_buf, 0, 500);

    char* buf = schedule->filter_buf;
    uint16_t new_action_list_len = 0;
    uint16_t filter_len = 0;
    UEP_Msg uep = copy_valid_action_to_buf(action, original_action_list_lens,
                                                 0x1900, 0xFF, buf, &new_action_list_len, &filter_len,
                                                 schedule->getConf());
    /* copy buf action to msg*/
    if(filter_len > 0) {
        for (int i = 0; i < new_action_list_len; ++i) {
            *(action + i) = schedule->filter_buf[i];
        }
        /* copy data to data */
        char* new_data_start = action + new_action_list_len;
        for (int i = 0; i < original_data_lens; ++i) {
            *(new_data_start + i) = original_data_satrt[i];
        }
        header->length = htons(ntohs(header->length) - filter_len);
        packet_out->actions_len = htons(new_action_list_len);

        arg.priority = schedule->getConf()->getPriorityManager()->getUEPPriorityOfSplitK(uep.window, uep.uepid);
        arg.uepid = uep.uepid;
        if(arg.priority > 0.5) {
            arg.qid = PO_QUEUE_1_ID;
        }
        else{
            arg.qid = PO_QUEUE_2_ID;
        }
        /* Debug:priority */
      //  fprintf(stderr, "Packet OUT UEP:[%02X,%02X] WindowID:%u Number:%u Priority:%lf\n",arg.uepid.eid, arg.uepid.uid, uep.window, uep.number, arg.priority );
    }
    /* Debug: Print Payload */
//    if(filter_len > 0) {
//        fprintf(stderr, "Packout Payload New\n");
//        for (int i = 0; i < ntohs(header->length); ++i) {
//            fprintf(stderr, "%02X ", *(msg + i));
//            if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
//        }
//        fprintf(stderr, "\n");
//    }

    //packet_out-
    char* eth_data = action + ntohs(packet_out->actions_len);
    uint16_t *type = (uint16_t*)(eth_data + 12);
    uint16_t e_type = ntohs(*type);

    if(e_type == LLDP_TYPE) {
        arg.qid = LLDP_QUEUE_ID;
        arg.priority = 0xFF;
        arg.identify = header->xid;
        //fprintf(stderr, "\n\nLLDP\n\n");
        arg.max_wait_time = arg.process_time = 0;
    }
}
void do_ofp_packetin_processed(char* msg, OFP_Msg_Arg &arg, Schedule* schedule) {
    arg.qid = PI_QUEUE_ID;

    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    ofp_packet_in* packet_in = (ofp_packet_in*)msg;
    char* match = (char*)(&packet_in->match);

    uint16_t match_total_length = getMatchLenth(match);
    uint16_t pad_len = 2;

    char* eth_data = match + match_total_length + pad_len;
    uint16_t *type = (uint16_t*)(eth_data + 12);
    uint16_t e_type = ntohs(*type);
    if(e_type == LLDP_TYPE) {
        arg.qid = LLDP_QUEUE_ID;
        arg.priority = 99.9;
        arg.identify = header->xid;
        arg.max_wait_time = arg.process_time = 0;
    } else {
        arg.qid = PI_QUEUE_ID;
    }

}
//todo
OFP_Msg_Arg Schedule::getOFPMsgArg(char *msg) {
    OFP_Msg_Arg arg;
    struct openflow::ofp_header *header =
            (struct openflow::ofp_header *)(msg);
    arg.max_wait_time = arg.process_time = 1;
    arg.priority = 1;

    switch (header->type) {
        case openflow::OFPT_PACKET_IN: {
            do_ofp_packetin_processed(msg, arg, this);
            break;
        }
        case openflow::OFPT_FLOW_MOD: {
            do_ofp_flowmod_processed(msg, arg, this);
            break;
        }
        case openflow::OFPT_PACKET_OUT: {
            do_ofp_packetout_processed(msg, arg, this);
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
        case openflow::OFPT_FEATURES_REPLY: {

            char dpid= (*(msg + sizeof(struct openflow::ofp_header) + 7)) + 48;
            this->dp_id = dpid;
            this->get_dp_id = 1;
            uint32_t lens = strlen(this->resp_pipe);
            this->resp_pipe[lens] = this->dp_id;
            this->resp_pipe[lens+1] = 0;


            this->other->setDpid(this->dp_id);
            this->other->setGet_dp_id(1);
            break;
        }
        default: {
       //     if(header->type == openflow::OFPT_ECHO_REQUEST)
      //      fprintf(stderr, "%s Other UEP:[%02X,%02X] Msg_Type:%s Priority:%lf Identify:%lu \n",this->name, arg.uepid.eid, arg.uepid.uid, getOFPMsgType(header->type).c_str() , arg.priority, arg.identify);
            arg.qid = IMPORTANT_QUEUE_ID;
        }
    }
    arg.len = ntohs(header->length);
    return arg;
}

int32_t PolicyConfig::setupConf(uint16_t server_port) {
    this->server_port = server_port;
    initListenSocket();
    initPriorityManager();
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
//                    int32_t policy_len = temp->h.match_len + sizeof(temp->h);
//                    char *data = new char[policy_len];
//                    for (int i = 0; i < policy_len; ++i) {
//                        data[i] = buf[i];
//                    }
//                    Policy *policy = (Policy *) data;
//                    this->policies.push_back(policy);
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
std::string getOFPMsgType(uint8_t ofp_type) {
    using namespace rofl;
    switch (ofp_type)
    {
        /* Immutable messages. */
        case openflow::OFPT_HELLO:
            return "OFPT_HELLO";
            /* Symmetric message */
        case openflow::OFPT_ERROR:
            return  "OFPT_ERROR";        /* Symmetric message */
        case openflow::OFPT_ECHO_REQUEST:
            return "OFPT_ECHO_REQUEST"; /* Symmetric message */
        case openflow::OFPT_ECHO_REPLY:
            return "OFPT_ECHO_REPLY";   /* Symmetric message */
        case openflow::OFPT_EXPERIMENTER:
            return "OFPT_EXPERIMENTER"; /* Symmetric message */

            /* Switch configuration messages. */
        case openflow::OFPT_FEATURES_REQUEST:
            return "OFPT_FEATURES_REQUEST";   /* Controller/switch message */
        case openflow::OFPT_FEATURES_REPLY:
            return "OFPT_FEATURES_REPLY";     /* Controller/switch message */
        case openflow::OFPT_GET_CONFIG_REQUEST:
            return "OFPT_GET_CONFIG_REQUEST"; /* Controller/switch message */
        case openflow::OFPT_GET_CONFIG_REPLY:
            return "OFPT_GET_CONFIG_REPLY";    /* Controller/switch message */
        case openflow::OFPT_SET_CONFIG:
            return  "OFPT_SET_CONFIG";        /* Controller/switch message */

            /* Asynchronous messages. */
        case openflow::OFPT_PACKET_IN:
            return "OFPT_PACKET_IN"; /* Async message */
        case openflow::OFPT_FLOW_REMOVED:
            return "OFPT_FLOW_REMOVED"; /* Async message */
        case openflow::OFPT_PORT_STATUS:
            return "OFPT_PORT_STATUS";/* Async message */

            /* Controller command messages. */
        case openflow::OFPT_PACKET_OUT:
            return "OFPT_PACKET_OUT"; /* Controller/switch message */
        case openflow::OFPT_FLOW_MOD:
            return "OFPT_FLOW_MOD"; /* Controller/switch message */
        case openflow::OFPT_GROUP_MOD:
            return "OFPT_GROUP_MOD";  /* Controller/switch message */
        case openflow::OFPT_PORT_MOD:
            return "OFPT_PORT_MOD"; /* Controller/switch message */
        case openflow::OFPT_TABLE_MOD:
            return "OFPT_TABLE_MOD";  /* Controller/switch message */

            /* Multipart messages. */
        case openflow::OFPT_MULTIPART_REQUEST:
            return "OFPT_MULTIPART_REQUEST";
            /* Controller/switch message */
        case openflow::OFPT_MULTIPART_REPLY:
            return "OFPT_MULTIPART_REPLY";   /* Controller/switch message */

//        case openflow::OFPT_STATS_REQUEST:
//            return "OFPT_STATS_REQUEST";     /* Controller/switch message */
//        case openflow::OFPT_STATS__REPLY:
//            return "OFPT_STATS__REPLY";      /* Controller/switch message */

            /* Barrier messages. */
        case openflow::OFPT_BARRIER_REQUEST:
            return "OFPT_BARRIER_REQUEST";  /* Controller/switch message */
        case openflow::OFPT_BARRIER_REPLY:
            return "OFPT_BARRIER_REPLY"; /* Controller/switch message */

            /* Queue Configuration messages. */
        case openflow::OFPT_QUEUE_GET_CONFIG_REQUEST:
            return "OFPT_QUEUE_GET_CONFIG_REQUEST";/* Controller/switch message */
        case openflow::OFPT_QUEUE_GET_CONFIG_REPLY:
            return "OFPT_QUEUE_GET_CONFIG_REPLY";   /* Controller/switch message */

            /* Controller role change request messages. */
        case openflow::OFPT_ROLE_REQUEST:
            return  "OFPT_ROLE_REQUEST";/* Controller/switch message */
        case openflow::OFPT_ROLE_REPLY:
            return "OFPT_ROLE_REPLY";   /* Controller/switch message */

            /* Asynchronous message configuration. */
        case openflow::OFPT_GET_ASYNC_REQUEST:
            return "OFPT_GET_ASYNC_REQUEST"; /* Controller/switch message */
        case openflow::OFPT_GET_ASYNC_REPLY:
            return "OFPT_GET_ASYNC_REPLY";   /* Controller/switch message */
        case openflow::OFPT_SET_ASYNC:
            return "OFPT_SET_ASYNC";         /* Controller/switch message */

            /* Meters and rate limiters configuration messages. */
        case openflow::OFPT_METER_MOD:
            return "OFPT_METER_MOD"; /* Controller/switch message */
        default:
            break;
    }
}