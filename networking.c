#include "common.h"
#include "networking.h"

void dump(char *str) {
    printf("%s", str);
    //printf("\n");
    int i;
    for(i = 0; i < strlen(str); i++) {
        printf("%02x ", str[i]);
    }
    printf("\n");
}

void setup_listener(int *listener, struct sockaddr_in *listen_addr, int port, int connections) {
    *listener = socket(AF_INET, SOCK_STREAM, 0);
    if(*listener == -1) {
        err("Failed to create socket");
    }

    int optval = 1;
    if(setsockopt(*listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        err("Failed to set socket option");
    }
    
    memset(listen_addr, 0, sizeof(*listen_addr));
    listen_addr->sin_addr.s_addr = INADDR_ANY;
    listen_addr->sin_family = AF_INET;
    listen_addr->sin_port = htons(port);

    if(bind(*listener, (struct sockaddr *) listen_addr, sizeof(*listen_addr)) == -1) {
        err("Failed to bind to listener socket");
    }

    if(listen(*listener, connections) == -1) {
        err("Failed to listen on listener socket");
    }
}

void accept_connection(int *client_sock, int listener_sock, struct sockaddr_in *client_addr) {
    socklen_t socklen = sizeof(struct sockaddr);
    *client_sock = accept(listener_sock, (struct sockaddr *) client_addr, &socklen);
    if(*client_sock == -1) {
        err("Failed to accept client connection to proxy");
    }
}

Request *assemble_request(char *client_request) {
    Request *return_request = (Request *) malloc(sizeof(Request));
    if(!return_request)
        err("Failed to allocate memory for request");

    char *ptr = client_request;

    /* get the request method */
    int i;
    for(i = 0; !isspace(ptr[i]); i++) {
        return_request->method[i] = ptr[i];
    } 

    //printf("%s\n", return_request->method);

    /* get the host */
    if((ptr = strstr(client_request, "http://")) || (ptr = strstr(client_request, "https://"))) {
        if(strstr(client_request, "http://"))
            ptr += 7;
        else
            ptr += 8;
    }

    for(i = 0; ptr[i] != '/' && !isspace(ptr[i]); i++) {
        return_request->host[i] = ptr[i];
    }

    //printf("%s\n", return_request->host);

    /* get the resources */
    ptr += strlen(return_request->host);

    for(i = 0; !isspace(ptr[i]); i++) {
        return_request->resources[i] = ptr[i];
    }

    /* resource is '/' if there are no resources */
    if(strlen(return_request->resources) == 0) {
        return_request->resources[0] = '/';
    }

    //printf("%s\n", return_request->resources);

    /* get the HTTP type */
    ptr = strstr(client_request, " HTTP");
    if(!ptr)
        err("Not an HTTP request");
    
    ptr++;
    for(i = 0; !isspace(ptr[i]); i++) {
        return_request->HTTP_type[i] = ptr[i];
    }

    //printf("%s\n", return_request->HTTP_type);
    
    /* assemble the headers */
    ptr = strstr(client_request, "Host:");
    for(; *ptr != '\n'; ptr++)
        ;
    ptr++;
    strcpy(return_request->headers, "Host: ");
    strcat(return_request->headers, return_request->host);
    strcat(return_request->headers, "\n");
    strcat(return_request->headers, ptr);

    return return_request;
}

void exec_request(Request *request, int client_sock) {
    char *request_to_send = (char *) malloc(strlen(request->method) + strlen(request->resources) + 
    strlen(request->HTTP_type) + strlen(request->headers) + 5); // + 5 for ' ', '\n', "\r\n", and '\0'

    if(!request_to_send)
        err("Failed to allocate memory for request to remote host");

    /* assemble request to send to remote host */
    strcat(request_to_send, request->method);
    strcat(request_to_send, " ");
    strcat(request_to_send, request->resources);
    strcat(request_to_send, " ");
    strcat(request_to_send, request->HTTP_type);
    strcat(request_to_send, "\n");
    strcat(request_to_send, request->headers);
    strcat(request_to_send, "\r\n");

    printf("REQUEST TO SEND:\n");
    dump(request_to_send);

    /* send request to remote host */
    struct sockaddr_in target_host;
    int target_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(target_sock == -1) {
        printf("Failed to create socket to remote host\n");
        return;
    }

    struct hostent *target_addr = gethostbyname(request->host);
    if(!target_addr) {
        printf("Failed to locate host: %s\n", request->host);
        return;
    }

    memset(&target_host, 0, sizeof(target_host));
    target_host.sin_addr = *(struct in_addr *) target_addr->h_addr_list[0];
    target_host.sin_family = AF_INET;
    target_host.sin_port = htons(WEB_PORT);

    socklen_t len = sizeof(struct sockaddr);
    if(connect(target_sock, (struct sockaddr *) &target_host, len) == -1) {
        printf("Failed to connect to host: %s\n", request->host);
        return;
    }    

    /* send the request to the remote host */
    if(send(target_sock, request_to_send, strlen(request_to_send), 0) == -1) {
        printf("Failed to send request to remote host\n");
        return;
    }
    
    char recv_buffer[5000];
    memset(recv_buffer, 0, sizeof(recv_buffer));

    /* get the reply from the remote host */
    if(recv(target_sock, recv_buffer, sizeof(recv_buffer), 0) == -1) {
        printf("Failed to get data from remote host\n");
        return;
    }

    /* send the data to the client (the browser) */
    if(send(client_sock, recv_buffer, sizeof(recv_buffer), 0) == -1) {
        printf("Failed to send reply to the client\n");
        return;
    }

    printf("REPLY:\n%s\n", recv_buffer);

    free(request_to_send);
    close(target_sock);
}







