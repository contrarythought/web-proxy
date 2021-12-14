#include "common.h"
#include "networking.h"

int main(int argc, char **argv) {
    if(argc != 2) {
        usage(argv[0], "<port number>");
    }

    /* port to listen */
    int port = atoi(argv[1]);

    /* socket that responds to requests to the proxy server */
    int listen_sock = 0;

    /* address that listener socket binds to */
    struct sockaddr_in listen_addr;

    /* set up listener socket and address it binds to */
    setup_listener(&listen_sock, &listen_addr, port, 1); // 5 connections is just a placeholder

    char recv_buffer[5000], send_buffer[5000];
    int client_sock = 0; // socket used to communicate with client that connects to proxy
    struct sockaddr_in client_addr;
    socklen_t socksize = sizeof(struct sockaddr);

    /* start the proxy server */
    while(1) {
        printf("Awaiting connections on %s:%d\n", inet_ntoa(listen_addr.sin_addr), ntohs(listen_addr.sin_port));

        /* accept client connections that connect to the address binded to by listen socket */
        accept_connection(&client_sock, listen_sock, &client_addr);

        /* receive the client request to the proxy */
        if(recv(client_sock, recv_buffer, sizeof(recv_buffer), 0) == -1) {
            err("Failed to receive data from client");
        }

        printf("\nCLIENT REQUEST:\n%s", recv_buffer);

        /* gather the request elements that will be sent to the remote host from the proxy */
        Request *request = assemble_request(recv_buffer);

        /* send request to the remote host */
        exec_request(request, client_sock);

        /* need to delete break eventually */
        break;
    }

    close(client_sock);
    close(listen_sock);
    return 0;    
}
