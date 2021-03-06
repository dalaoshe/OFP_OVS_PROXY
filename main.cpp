#include <iostream>
#include "unp.h"
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

    TunArg* arg = (TunArg*)argv;
    int32_t client_fd = arg->client_fd;
    int32_t server_fd = arg->server_fd;
    pthread_t server_tid = arg->server_tid;
    uint16_t port = arg->port;
    Schedule* client_to_server_schedule = arg->client_to_server_schedule;

    char buf[65539];
    int count = 0;
    char endpoint[] = "client";
    while (1) {
        //memset(buf, 0, sizeof(buf));
        //int cn = read(client_fd, buf, 2048);

        int cn = read_ofp_msg(client_fd, buf, port, endpoint);
        if (cn < 0) {
            fprintf(stderr, "port:%d read client sockfd %d error: %s \n",port, client_fd, strerror(errno));
            Close(client_fd);
            Close(server_fd);

            if(pthread_kill(server_tid, SIGUSR1) != 0) {
                fprintf(stderr, "port:%d, kill server thread %lu error: %s \n",port, server_tid, strerror(errno));
            }
            fprintf(stderr, "kill server thread %lu error: %s \n", server_tid, strerror(errno));
            break;
        } else if (cn == 0) {
            fprintf(stderr, "port:%d client try to close sockfd %d \n",port, client_fd);
            Close(client_fd);
            Close(server_fd);
            if(pthread_kill(server_tid, SIGUSR1) != 0) {
                fprintf(stderr, "kill server thread %lu error: %s \n", server_tid, strerror(errno));
            }
            fprintf(stderr, "kill server thread %lu error: %s \n", server_tid, strerror(errno));
            break;
        } else {
            client_to_server_schedule->putMessage(buf, cn, server_fd);
            //Writev_nByte(server_fd, buf, cn);
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
    int32_t client_fd = arg->client_fd;
    int32_t server_fd = arg->server_fd;
    int16_t port = arg->port;
    Schedule* server_to_client_schedule = arg->server_to_client_schedule;

    char buf[65539];
    signal(SIGUSR1, signal_handler);
    char endpoint[] = "server";
    while(1) {
        int sn = read_ofp_msg(server_fd, buf, port, endpoint);
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
            server_to_client_schedule->putMessage(buf, sn, client_fd);
            //Writev_nByte(client_fd, buf, sn);
        }
    }
}

void* run_config_daemon(void* argv) {
    PolicyConfig* conf = (PolicyConfig*) argv;
    //conf->listenRequest();
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

    Listen(listenTunnel, 10);
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
            /* Child Process             */

            int server_fd = Socket(AF_INET, SOCK_STREAM, 0);
            Socket_Peer_Connect(server_fd, (SA*)&server, len);

            /* Forbidden Nagle's algorithm */
            SetSocket(server_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
            SetSocket(client_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
            SetSocket(client_fd, IPPROTO_TCP, TCP_QUICKACK, &on, sizeof(on));
            SetSocket(server_fd, IPPROTO_TCP, TCP_QUICKACK, &on, sizeof(on));

            fprintf(stderr, "port:%d tunnel connect server ok, sockfd is %d\n",ntohs(client.sin_port), server_fd);

            /* Start Policy Config Daemon Thread To Receive Policy */
            PolicyConfig* policyConfig = new PolicyConfig();
            policyConfig->setupConf(ntohs(client.sin_port));
            pthread_t p_t;
            pthread_create(&p_t, NULL, &run_config_daemon, (void*)policyConfig);




            fprintf(stderr, "port:%d start schedule\n", ntohs(client.sin_port));
            /* Start Schedule Thread To Process Client_To_Server Msg and Server_To_Client Msg */
            timeval now;
            gettimeofday(&now, NULL);
            Schedule* client_to_server_schedule = new Schedule("CLIENT", "/home/dalaoshe/Client_to_Server_PIPE.txt", ntohs(client.sin_port));
            Schedule* server_to_client_schedule = new Schedule("SERVER", "/home/dalaoshe/SERVER_to_Client_PIPE.txt", ntohs(client.sin_port) + now.tv_usec);
            client_to_server_schedule->other = server_to_client_schedule;
            server_to_client_schedule->other = client_to_server_schedule;

            client_to_server_schedule->setOtherQueue(server_to_client_schedule->getQueues());
            server_to_client_schedule->setOtherQueue(client_to_server_schedule->getQueues());

            pthread_t s1, s2;
            ScheduleArg arg1, arg2;
            arg1.schedule = client_to_server_schedule;
            arg2.schedule = server_to_client_schedule;
            arg2.config = arg1.config = policyConfig;
            pthread_create(&s1, NULL, &schedule_thread, (void*)&arg1);
            pthread_create(&s2, NULL, &schedule_thread, (void*)&arg2);
            fprintf(stderr,"port:%d schedule ready\n",ntohs(client.sin_port));


            /* Start Thread To Receive Msg */
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


            fprintf(stderr, "port:%d tunnel wait client_thread close \n",ntohs(client.sin_port));
            pthread_join(t_client, NULL);

            fprintf(stderr, "port:%d tunnel wait server_thread close \n",ntohs(client.sin_port));
            pthread_join(t_server, NULL);

            fprintf(stderr, "port:%d tunnel close \n",ntohs(client.sin_port));
            delete server_to_client_schedule;
            delete client_to_server_schedule;
            exit(0);
        }

    }


}