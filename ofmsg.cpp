//
// Created by dalaoshe on 17-7-15.
//


#include "ofmsg.h"

using namespace rofl;

void parse_message(char* buf){
    struct rofl::openflow::ofp_header *hdr =
            (struct rofl::openflow::ofp_header *)buf;

//    if (rxbuffer.length() < sizeof(struct rofl::openflow::ofp_header)) {
//        throw eBadRequestBadLen("eBadRequestBadLen", __FILE__, __FUNCTION__,
//                                __LINE__);
//    }

    /* make sure to have a valid cofmsg* msg object after parsing */
//    switch (hdr->version) {
//        case rofl::openflow10::OFP_VERSION: {
//            parse_of10_message(&msg);
//        } break;
//        case rofl::openflow12::OFP_VERSION: {
//            parse_of12_message(&msg);
//        } break;
//        case rofl::openflow13::OFP_VERSION: {
//            parse_of13_message(&msg);
//        } break;
//        default: {
//            throw eBadRequestBadVersion("eBadRequestBadVersion", __FILE__,
//                                        __FUNCTION__, __LINE__);
//        };
//    }
//    switch (hdr->type) {
//        case rofl::openflow::ofp_type::OFPT_PACKET_IN: {
//
//            break;
//        }
//    }


};
int read_ofp_msg(int fd, char* buf, pthread_t tid, char* endpoint) {
    /*
 * receiving messages
 */
    int count = 0;
    // fragment pending
    bool rx_fragment_pending = true;

    // incomplete fragment message fragment received in last round
 //   cmemory rxbuffer;

    // number of bytes already received for current message fragment
    unsigned int msg_bytes_read = 0;

    // read not more than this number of packets per round before rescheduling
    unsigned int max_pkts_rcvd_per_round;

    struct timeval t, t_2;


    ssize_t rc = 0;
    //t_start = time(NULL) ;
    while (rx_fragment_pending) {
        count ++;
        uint16_t msg_len = 0;
//        /* how many bytes do we have to read? */
        if (msg_bytes_read < sizeof(struct openflow::ofp_header)) {
            msg_len = sizeof(struct openflow::ofp_header);
        } else {
            //struct openflow::ofp_header *header =
            //        (struct openflow::ofp_header *)(rxbuffer.somem());
            struct openflow::ofp_header *header =
                    (struct openflow::ofp_header *)(buf);
            msg_len = be16toh(header->length);
        }

//        /* sanity check: 8 <= msg_len <= 2^16 */
        if (msg_len < sizeof(struct openflow::ofp_header)) {
            fprintf(stderr, "%s:port:%d read first segment < ofp_header\n",endpoint, tid);
            goto on_error;
        }

        rc = read(fd, (void *)(buf + msg_bytes_read),
                  msg_len - msg_bytes_read);//, MSG_DONTWAIT);
        if(count == 1)gettimeofday( &t, NULL );
        //msg_bytes_read = rc;
       //fprintf(stderr, "need read fd:%d byte:%d, actuall read %d count:%d tid:%d\n",fd, msg_len-msg_bytes_read, rc,count, tid);

        if (rc < 0) {
            fprintf(stderr, "%s:port:%d read rc < 0, ERROR:%s\n",endpoint, tid,strerror(errno));
            switch (errno) {
                case EAGAIN: {
                    return rc;
                }
                    break;
                default: {
                    goto on_error;
                };
            }
        } else if (rc == 0) {
            fprintf(stderr, "%s:port:%d read rc == 0, close\n",endpoint, tid);
            goto on_error;
        }

        msg_bytes_read += rc;

        /* minimum message length received, check completeness of message */
        if (msg_bytes_read >= sizeof(struct openflow::ofp_header)) {
//            struct openflow::ofp_header *header =
//                    (struct openflow::ofp_header *)(rxbuffer.somem());
            struct openflow::ofp_header *header =
                    (struct openflow::ofp_header *)(buf);
            msg_len = be16toh(header->length);
            uint32_t xid = header->xid;
            /* ok, message was received completely */
            if (msg_len == msg_bytes_read) {
                rx_fragment_pending = false;
                parse_message(buf);
                switch (header->type) {
                    case rofl::openflow::OFPT_PACKET_IN: {
                        //sleep(3000);
                        gettimeofday( &t_2, NULL );
                        double ms = (double (t_2.tv_sec-t.tv_sec) * 1000.0) + (double (t_2.tv_usec - t.tv_usec) / 1000.0);
                      //  printf("time: %d us\n", timeuse);


                        fprintf(stderr, "\n\n\n%s:port:%d read PACKET_IN, use time %.2f ms, "
                                "mes-head-len is :%d, read:%d times, TransactionID:%u \n\n\n",
                                endpoint, tid,ms,be16toh(header->length), count, xid);
                        break;
                    }
                    case rofl::openflow::OFPT_FLOW_MOD: {
                        gettimeofday( &t_2, NULL );
                        double ms = (double (t_2.tv_sec-t.tv_sec) * 1000.0) + (double (t_2.tv_usec - t.tv_usec) / 1000.0);
                        //  printf("time: %d us\n", timeuse);

//                        fprintf(stderr, "\n\n\n%s:port:%d read FLOW_MOD, use time %.2f ms, "
//                                "mes-head-len is :%d, read:%d times, TransactionID:%u\n\n\n\n",
//                                endpoint, tid,ms,be16toh(header->length), count, xid);
                        break;
                    }

                    default: {
                        gettimeofday( &t_2, NULL );
                        double ms = (double (t_2.tv_sec-t.tv_sec) * 1000.0) + (double (t_2.tv_usec - t.tv_usec) / 1000.0);
                        //printf("time: %d us\n", timeuse);
//                        if(msg_bytes_read > 8)
//                        fprintf(stderr, "\n\n\n%s:port:%d, read MSG_TYPE:%d, use time %.2f ms, mes-head-len is :%d, read:%d times, TransactionID:%u\n\n\n",
//                                endpoint, tid,header->type, ms,
//                                be16toh(header->length), count, xid);
                        break;
                    }
                }

               // msg_bytes_read = 0;
                break;
            } else {
                rx_fragment_pending = true;
            }
            //break;
        }
    }

    return msg_bytes_read;

    on_error:

    //fprintf(stderr, "port:%d recv ofp_mesg error, error:%s\n",tid, strerror(errno));
    return rc;
//    switch (state) {
//        case STATE_TCP_ESTABLISHED: {
//            VLOG(2) << __FUNCTION__ << " TCP: peer shutdown laddr=" << laddr.str()
//                    << " raddr=" << raddr.str();
//            close();
//
//            if (flag_test(FLAG_RECONNECT_ON_FAILURE)) {
//                backoff_reconnect(true);
//            }
//
//            try {
//                crofsock_env::call_env(env).handle_closed(*this);
//            } catch (std::runtime_error &e) {
//                VLOG(1) << __FUNCTION__ << "() caught runtime error, what: %s" << e.what()
//                        << " laddr=" << laddr.str() << " raddr=" << raddr.str();
//            }
//            // WARNING: handle_closed might delete this socket, don't call anything here
//        } break;
//        default: { VLOG(2) << __FUNCTION__ << " error in state=" << state; };
//    }
}