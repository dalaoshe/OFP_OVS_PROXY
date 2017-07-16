#include <iostream>
#include "unp.h"
#include "ofmsg.h"
#include "schedule.h"

using namespace rofl;
void do_tcp_tunnel(char* serverip, char* serverport, char* tunnelport);

int main(int argc, char** argv) {
    std::cout << "Hello, World!" << std::endl;
    do_tcp_tunnel(argv[2], argv[3], argv[4]);
    return 0;
}

struct TunArg{
    int client_fd;
    int server_fd;
    pthread_t server_tid;
    uint16_t port;
    Schedule* client_to_server_schedule;
    Schedule* server_to_client_schedule;
    struct timeval *t, *t_2;
};

void* read_client(void* argv) {
   // pthread_detach(pthread_self());
    TunArg* arg = (TunArg*)argv;
    int32_t client_fd = arg->client_fd;
    int32_t server_fd = arg->server_fd;
    pthread_t server_tid = arg->server_tid;
    uint16_t port = arg->port;
    Schedule* client_to_server_schedule = arg->client_to_server_schedule;

    char buf[65539];
    int count = 0;
    while (1) {
        //memset(buf, 0, sizeof(buf));
        //int cn = read(client_fd, buf, 2048);
        int cn = read_ofp_msg(client_fd, buf, port, "client");
        if (cn < 0) {
            fprintf(stderr, "port:%d read client sockfd %d error: %s \n",port, client_fd, strerror(errno));
            Close(client_fd);
            Close(server_fd);

            if(pthread_kill(server_tid, SIGUSR1) != 0) {
                fprintf(stderr, "port:%d, kill server thread %d error: %s \n",port, server_tid, strerror(errno));
            }
            fprintf(stderr, "kill server thread %d error: %s \n", server_tid, strerror(errno));
            break;
        } else if (cn == 0) {
            fprintf(stderr, "port:%d client try to close sockfd %d \n",port, client_fd);
            Close(client_fd);
            Close(server_fd);
            if(pthread_kill(server_tid, SIGUSR1) != 0) {
                fprintf(stderr, "kill server thread %d error: %s \n", server_tid, strerror(errno));
            }
            fprintf(stderr, "kill server thread %d error: %s \n", server_tid, strerror(errno));
            break;
        } else {
//            struct openflow::ofp_header *header =
//                    (struct openflow::ofp_header *)(buf);

            Write_nByte(server_fd, buf, cn);
            //client_to_server_schedule->putMessage(buf, cn, server_fd);
//            if(header->type == openflow::OFPT_FLOW_MOD || header->type == openflow::OFPT_PACKET_IN) {
//                timeval told = *arg->t;
//                gettimeofday( arg->t, NULL );
//                double ms = (double (arg->t->tv_sec-told.tv_sec) * 1000.0) + (double (arg->t->tv_usec - told.tv_usec) / 1000.0);
//                fprintf(stderr, "client:port:%d read %d byte from client by client_fd %d to server_fd %d last packetin time:%lf ms\n", port, cn,
//                        client_fd, server_fd, ms);
//            }
          //  int w = write(server_fd, buf, cn);
        }
    }

}

void signal_handler(int signum) {
    if(signum == SIGUSR1) {
        pthread_exit(NULL);
    }
}


void* read_server(void* argv) {
    TunArg* arg = (TunArg*)argv;
    int client_fd = arg->client_fd;
    int server_fd = arg->server_fd;
    int port = arg->port;
    char buf[65539];
    signal(SIGUSR1, signal_handler);
    while(1) {
        int sn = read_ofp_msg(server_fd, buf, port, "server");
        if (sn < 0) {
            fprintf(stderr, "port:%d read server sockfd %d error: %s \n",port, server_fd, strerror(errno));
            Close(client_fd);
            Close(server_fd);
            break;
        } else if (sn == 0) {
            fprintf(stderr, "port:%d server try to close close sockfd %d \n",port, server_fd);
            Close(client_fd);
            Close(server_fd);
            break;
        } else {
            struct openflow::ofp_header *header =
                    (struct openflow::ofp_header *)(buf);
            Write_nByte(client_fd, buf, sn);
            if(header->type == openflow::OFPT_FLOW_MOD || header->type == openflow::OFPT_PACKET_IN) {
                gettimeofday( arg->t_2, NULL );
                double ms = (double (arg->t_2->tv_sec-arg->t->tv_sec) * 1000.0) + (double (arg->t_2->tv_usec - arg->t->tv_usec) / 1000.0);
                fprintf(stderr, "server:port:%d read %d byte from server by server_fd %d to client_fd %d user:%lf ms\n", port, sn,
                        server_fd, client_fd, ms);
            }
        }
    }
}

void do_tcp_tunnel(char* serverip, char* serverport, char* tunnelport) {
    sockaddr_in server,client,tunnel;
    server.sin_family = client.sin_family = tunnel.sin_family = AF_INET;

    server.sin_port = htons(atoi(serverport));
    tunnel.sin_port = htons(atoi(tunnelport));

    Inet_pton(AF_INET, serverip, &(server.sin_addr.s_addr));
    tunnel.sin_addr.s_addr = INADDR_ANY;

    socklen_t len = sizeof(sockaddr_in);


    int listenTunnel = Socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    SetSocket(listenTunnel, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    Bind_Socket(listenTunnel, (SA*)&tunnel, sizeof(tunnel));

    Listen(listenTunnel, 4);
    fprintf(stderr, "read to listen\n");



    while(1) {
        int client_fd =Accept(listenTunnel, (SA*)&client, &len);
        if(client_fd < 0) break;
        char client_ip[20];
        Inet_ntop(AF_INET, (char*)&client.sin_addr.s_addr, client_ip, sizeof(client_ip));
        fprintf(stderr, "tunnel recv connect from ip:%s, port:%u\n", client_ip, ntohs(client.sin_port));

        if(fork() != 0) {
            Close(client_fd);
            continue;
        }
        else {


            int server_fd = Socket(AF_INET, SOCK_STREAM, 0);
            Socket_Peer_Connect(server_fd, (SA*)&server, len);

            SetSocket(server_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
            SetSocket(client_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

            fprintf(stderr, "port:%d tunnel connect server ok, sockfd is %d\n",ntohs(client.sin_port), server_fd);

            //start schedule
            Schedule* client_to_server_schedule = new Schedule();
            Schedule* server_to_client_schedule = new Schedule();
            pthread_t s1, s2;
            ScheduleArg arg1, arg2;
            arg1.schedule = client_to_server_schedule;
            arg2.schedule = server_to_client_schedule;
            pthread_create(&s1, NULL, &schedule_thread, (void*)&arg1);
            pthread_create(&s2, NULL, &schedule_thread, (void*)&arg2);
            fprintf(stderr,"port:%d schedule ready\n",ntohs(client.sin_port));


            TunArg tun;
            struct timeval t, t_2;

            tun.server_fd = server_fd; tun.client_fd = client_fd; tun.port = ntohs(client.sin_port);
            tun.t = &t; tun.t_2 = &t_2;
            tun.client_to_server_schedule = client_to_server_schedule;
            tun.server_to_client_schedule = server_to_client_schedule;

            pthread_t t_client, t_server;


            pthread_create(&t_server, NULL, &read_server, (void*)&tun);

            tun.server_tid = t_server;
            pthread_create(&t_client, NULL, &read_client, (void*)&tun);

            fprintf(stderr, "port:%d tunnel wait server_thread close \n",ntohs(client.sin_port), strerror(errno));
            pthread_join(t_server, NULL);
            fprintf(stderr, "port:%d tunnel wait client_thread close \n",ntohs(client.sin_port), strerror(errno));
            pthread_join(t_client, NULL);
            fprintf(stderr, "port:%d tunnel close \n",ntohs(client.sin_port), strerror(errno));

            exit(0);
        }

    }


}