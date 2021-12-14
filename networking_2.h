#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h> /* for looking up hosts via DNS (struct hostent, gethostbyname()) */

#define WEB_PORT 80
#define GET_REQ "GET "
#define POST_REQ "POST "
#define PUT_REQ "PUT "

#define HTTP_FRAME "http://"
#define WWW_FRAME "www."

typedef struct Request {
    char method[5];
    char host[500];
    char resources[500];
    char HTTP_type[10];
    char headers[1000];
} Request;

enum REQUEST_TYPE {
    GET = 1,
    POST,
    PUT,
    HEAD,
    CONNECT
};

void dump(char *str);
void setup_listener(int *listener, struct sockaddr_in *listen_addr, int port, int connections);
void accept_connection(int *client_sock, int listener_sock, struct sockaddr_in *client_addr);
Request *assemble_request(char *client_request);
void exec_request(Request *request, int client_sock);

#endif
