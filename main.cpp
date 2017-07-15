#include <iostream>
#include "unp.h"
#include "ofmsg.h"

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
};

void* read_client(void* argv) {
   // pthread_detach(pthread_self());
    TunArg* arg = (TunArg*)argv;
    int client_fd = arg->client_fd;
    int server_fd = arg->server_fd;
    pthread_t server_tid = arg->server_tid;
    uint16_t port = arg->port;
    char buf[65539];
    int count = 0;
    while (1) {
        memset(buf, 0, sizeof(buf));
        //int cn = read(client_fd, buf, sizeof(buf));
        int cn = read_ofp_msg(client_fd, buf, port);
        if (cn < 0) {
            fprintf(stderr, "read client sockfd %d error: %s \n", client_fd, strerror(errno));
            Close(client_fd);
            Close(server_fd);

            if(pthread_kill(server_tid, SIGUSR1) != 0) {
                fprintf(stderr, "kill server thread %d error: %s \n", server_tid, strerror(errno));
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
            Write_nByte(server_fd, buf, cn);
          //  int w = write(server_fd, buf, cn);
        }
    }

}

void signal_handler(int signum) {
//    fprintf(stderr, "server signal\n");
    if(signum == SIGUSR1) {
        pthread_exit(NULL);
    }
  //  Close(server_arg->server_fd);

}


void* read_server(void* argv) {
    TunArg* arg = (TunArg*)argv;
    int client_fd = arg->client_fd;
    int server_fd = arg->server_fd;
    char buf[65539];
    signal(SIGUSR1, signal_handler);
    while(1) {
        memset(buf, 0, sizeof(buf));
        int sn = read(server_fd, buf, sizeof(buf));
        if (sn < 0) {
            fprintf(stderr, "read server sockfd %d error: %s \n", server_fd, strerror(errno));
            Close(client_fd);
            Close(server_fd);
            break;
        } else if (sn == 0) {
            fprintf(stderr, "server try to close close sockfd %d \n", server_fd);
            Close(client_fd);
            Close(server_fd);
            break;
        } else {
            Write_nByte(client_fd, buf, sn);
        }
    }
}

void do_tcp_tunnel(char* serverip, char* serverport, char* tunnelport) {
    sockaddr_in server,client,tunnel;
    server.sin_family = client.sin_family = tunnel.sin_family = AF_INET;

    server.sin_port = htons(atoi(serverport));
    tunnel.sin_port = htons(atoi(tunnelport));

    Inet_pton(AF_INET, serverip, &(server.sin_addr.s_addr));
//    server.sin_addr.s_addr = INADDR_ANY;
    tunnel.sin_addr.s_addr = INADDR_ANY;

    socklen_t len = sizeof(sockaddr_in);

    int listenTunnel = Socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    SetSocket(listenTunnel, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    Bind_Socket(listenTunnel, (SA*)&tunnel, sizeof(tunnel));

    Listen(listenTunnel, 4);
    printf("read to listen\n");

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
            fprintf(stderr, "tunnel connect server ok \n");


            TunArg tun; tun.server_fd = server_fd; tun.client_fd = client_fd; tun.port = ntohs(client.sin_port);
            pthread_t t_client, t_server;
            pthread_create(&t_server, NULL, &read_server, (void*)&tun);
            tun.server_tid = t_server;

            pthread_create(&t_client, NULL, &read_client, (void*)&tun);

            fprintf(stderr, "tunnel wait server_thread close \n", strerror(errno));
            pthread_join(t_server, NULL);
            fprintf(stderr, "tunnel wait client_thread close \n", strerror(errno));
            pthread_join(t_client, NULL);
            fprintf(stderr, "tunnel close \n", strerror(errno));
            exit(0);
        }

    }


}