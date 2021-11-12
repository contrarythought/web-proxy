#include "common.h"
#include "networking.h"

int HTTP_req(char *request);
int find_req_type(char *request);
void execute_request(int req_type, char *request, int client_fd, char *DNS);
void exec_get(); // TODO
void exec_head();
int correct_addr_form(char *addr);

int main(int argc, char **argv) {
    if(argc != 2) {
        usage(argv[0], "Port number");
    }

    /** port number of web proxy that client connects to */
    int port = atoi(argv[1]);

    /** socket that will listen to client HTTP requests */
    int proxy_listener = socket(AF_INET, SOCK_STREAM, 0);
    if(proxy_listener == -1) {
        err("creating socket");
    }

    /** resuse socket. This allows socket to bind to a given port
     * even if port is seemingly already in use (if socket not closed properly)
     */
    int optval = 1;
    if(setsockopt(proxy_listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) {
        err("setting socket option");
    }

    /** address of web proxy. This will take in requests from client, send them to
        remote target, receive response from remote host, and send the response to the client */
    struct sockaddr_in proxy;
    memset(&proxy, 0, sizeof(proxy));
    proxy.sin_family = AF_INET;
    proxy.sin_addr.s_addr = INADDR_ANY;
    proxy.sin_port = htons(port);

    /* bind the c2p socket to address of proxy */
    if(bind(proxy_listener, (struct sockaddr *) &proxy, sizeof(proxy)) == -1) {
        err("binding");
    }

    /* tell the socket to listen to requests on proxy's address */
    if(listen(proxy_listener, 1) == -1) {
        err("listening");
    }

    /* start up the proxy server */
    char recv_buffer[5000], send_buffer[5000];
    while(1) {
        int client_to_proxy;
        socklen_t len = sizeof(struct sockaddr);
        struct sockaddr_in client;

        printf("listening on %s on port: %d\n", inet_ntoa(proxy.sin_addr), ntohs(proxy.sin_port));

        /* accept a client request to the proxy server */
        client_to_proxy = accept(proxy_listener, (struct sockaddr *) &client, &len);
        if(client_to_proxy == -1) {
            err("accepting client request to proxy");
        }

        memset(send_buffer, 0, sizeof(send_buffer));
        strcpy(send_buffer, "Web proxy\n");
        if(send(client_to_proxy, send_buffer, strlen(send_buffer), 0) == -1) {
            err("sending data to client");
        }
        
        memset(recv_buffer, 0, sizeof(recv_buffer));
        if(recv(client_to_proxy, recv_buffer, sizeof(recv_buffer) - 1, 0) == -1) {
            err("receiving data from client");
        }
        // variables to pass to the actual request function
        char *token = strtok(recv_buffer, " ");
        int req_type;
        char *DNS = NULL;
        for (int i=0;i<3 && token;i++) {
            printf("%s\n",token);            
            if (i==0) {
                req_type = find_req_type(token);
                if(req_type == -1) {
                    printf("Not a correct request type\n");
                    break; // TODO: we gotta break from 2 loops, lets handle that later, segfaults are more fancy
                }
            }else if (i==1) {
                DNS = token;
                // correct_addr_form() returns 0 if http://www., and returns 1 if www. Returns -1 if none are detected
                int corr_addr = correct_addr_form(DNS); 
                if(corr_addr == -1) {
                    printf("Invalid DNS\n");
                    strcpy(send_buffer, "Invalid DNS");
                    if(send(client_to_proxy, send_buffer, strlen(send_buffer), 0) == -1) {
                        printf("Failed to send data to client\n");
                    }
                    break;
                }else if(corr_addr == 0) {
                    printf("TOKEN TO PARSE: %s\n", token);
                   // char *blank = strtok(token, "http://");
                    DNS = strtok(token, "http://");
                    printf("DNS: %s\n", DNS);
                }
            }else{
                if(!HTTP_req(token)) {
                    printf("Not an HTTP request\n");
                    break;
                }
            }
            token = strtok(NULL, " ");
            printf("TOKEN: %s\n", token);
        }
        
        printf("EXITED LOOP\n");
        /* execute the request type */
        execute_request(req_type, recv_buffer, client_to_proxy, DNS);

        // NEED TO DELETE THIS BREAK EVENTUALLY
        break;
    }

    close(proxy_listener);
    return EXIT_SUCCESS;
}

int correct_addr_form(char *request) {
    // TODO: I broke this functions somehow
    printf("Request: %s\n", request);
    char *ptr = NULL;
    if((ptr = strstr(request, HTTP_FRAME))) {
        printf("HTTP detected\n");
        return 0;
    }
    if((ptr = strstr(request, WWW_FRAME))) {
        printf("WWW detected\n");
        return 1;
    }

    printf("Failed to detect \"http://\" or \"www.\"\n");
    return -1;
    //return 1;
}

// TODO
void execute_request(int req_type, char *request, int client_fd, char *DNS) {
    printf("Whole request: %s\n", DNS);
    /* gethostbyname() returns a struct hostent * */
    struct hostent *target_info = NULL;

    /* remote host address that sock_fd will connect to */
    struct sockaddr_in remote_host;

    /* socket that will connect to the address provided by target_info */
    int sock_fd;

    /* extract the DNS for use in gethostbyname() */
    char send_buf[5000];
    memset(send_buf, 0, sizeof(send_buf));

    //int DNS_LEN = 0;
    switch(req_type) {
        case GET: /* TODO: PUT IN SEPARATE FUNCTION */           
            /*
            ptr = request + strlen(GET_REQ);
            int i;
            for(i = 0; !isspace(*(ptr + i)); i++, DNS_LEN++)
                ;
            DNS = (char *) malloc(DNS_LEN + 1);
            for(i = 0; i < DNS_LEN; i++) {
                DNS[i] = *(ptr + i);
            }
            */
            
            /* now that I have the DNS, I just need to retrieve the html string of that website, and then return it 
                to the client */
            printf("%s\n", DNS);
            target_info = gethostbyname(DNS);
            if(target_info == NULL) {
                printf("Failed to retrieve remote host address\n");
                return;
            }

            /* fill the socket address with info that the socket can use to connect to */
            memset(&remote_host, 0, sizeof(remote_host));
            remote_host.sin_addr = *(struct in_addr *) target_info->h_addr_list[0];
            remote_host.sin_family = AF_INET;
            remote_host.sin_port = htons(WEB_PORT);

            sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if(sock_fd == -1) {
                printf("Failed to create socket to connect to remote host\n");
                return;
            }
            
            if(connect(sock_fd, (struct sockaddr *) &remote_host, sizeof(remote_host)) == -1) {
                printf("Failed to connect to %s\n", DNS);
                memset(send_buf, 0, sizeof(send_buf));
                sprintf(send_buf, "Failed to connect to %s\n", DNS);
                if(send(client_fd, send_buf, strlen(send_buf), 0) == -1) {
                    printf("Failed to send data to client\n");
                    return;
                }
                return;
            }
            
            /* send the actual GET request to the remote host, and then return the HTML string */
            if(send(sock_fd, "GET / HTTP/1.1\r\n\r\n", strlen("GET / HTTP/1.1\r\n\r\n"), 0) == -1) {
                printf("Failed to send request to remote host\n");
                return;
            }

            char recv_buffer[5000];
            if(recv(sock_fd, recv_buffer, sizeof(recv_buffer), 0) == -1) {
                printf("Failed to read reply from remote host\n");
                return;
            }

            printf("%s\n", recv_buffer);
            memset(send_buf, 0, sizeof(send_buf));
            if(send(client_fd, recv_buffer, strlen(recv_buffer), 0) == -1) {
                printf("Failed to send response to client\n");
                return;
            }
            break;
        //case HEAD:
            //exec_head();
            //break;
    }

}

int find_req_type(char *request) {
    // TODO: as the request type is now separated I guess we can just do ifs for GET, POST, etc...
    char *ptr = request; // pointing at beginning of request
    int i, correct_req = 1;
    switch(*ptr) {
        case 'P':
            for(i = 1; isalpha(*(ptr + i)); i++) {
                if(ptr[i] != POST_REQ[i]) {
                    correct_req = 0;
                    break;
                }
            }
            if(correct_req)
                return POST;
            correct_req = 1;
            for(i = 1; isalpha(*(ptr + i)); i++) {
                if(ptr[i] != PUT_REQ[i]) {
                    correct_req = 0;
                    break;
                }
            }
            if(correct_req)
                return PUT;
            else 
                return -1;
        case 'G':
            for(i = 1; isalpha(*(ptr + i)); i++) {
                if(ptr[i] != GET_REQ[i])
                    return -1;
            }
            return GET;
        case 'H':
            for(i = 1; isalpha(*(ptr + i)); i++) {
                //if(ptr[i] != HEAD_REQ[i])
                    return -1;
            }
            return -1;
            //return HEAD;
    }
    return -1;
}

int HTTP_req(char *request) {
    if (request=="HTTP/1.0") {
        return 1;
    }else if (request=="HTTP/1.1"){
        return 1;
    }else{
        return 0;
    }
    /*
    char *ptr;
    ptr = strstr(request, "HTTP/1.0");
    if(ptr) {
        return 1;
    }

    ptr = strstr(request, "HTTP/1.1");
    if(ptr) {
        return 1;
    }
    return 0;
    */
}
