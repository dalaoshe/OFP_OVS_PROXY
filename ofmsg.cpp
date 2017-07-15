//
// Created by dalaoshe on 17-7-15.
//


#include "ofmsg.h"

using namespace rofl;

void parse_message(){

};
int read_ofp_msg(int fd, char* buf, pthread_t tid) {
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

    ssize_t rc = 0;

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
            fprintf(stderr, "read first segment < ofp_header\n");
            goto on_error;
        }

        rc = read(fd, (void *)(buf + msg_bytes_read),
                  msg_len - msg_bytes_read);//, MSG_DONTWAIT);
        //msg_bytes_read = rc;
       //fprintf(stderr, "need read fd:%d byte:%d, actuall read %d count:%d tid:%d\n",fd, msg_len-msg_bytes_read, rc,count, tid);

        if (rc < 0) {
            fprintf(stderr, "read rc < 0 \n");
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
            fprintf(stderr, "read rc == 0 \n");
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

            /* ok, message was received completely */
            if (msg_len == msg_bytes_read) {
                rx_fragment_pending = false;
                parse_message();
                fprintf(stderr, "port:%d mes-head-len is :%d, read:%d times, read:%d byte\n",tid, be16toh(header->length), count, msg_bytes_read);
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

    fprintf(stderr, "recv ofp_mesg error, error:%s\n", strerror(errno));
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