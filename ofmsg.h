//
// Created by dalaoshe on 17-7-15.
//

#ifndef TCP_TUNNEL_OFMSG_H
#define TCP_TUNNEL_OFMSG_H

#include "openflow_common.h"
#include "unp.h"

class ofmsg {
    char* buf;
public:
    ofmsg() {
        this->buf = new char [63358];
    }
    ~ofmsg() {
        delete this->buf ;
    }
};

int read_ofp_msg (int fd, char* buf, pthread_t tid);


#endif //TCP_TUNNEL_OFMSG_H
