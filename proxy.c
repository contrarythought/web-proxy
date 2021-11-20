#include "common.h"
#include "networking.h"

int HTTP_req(char *request);
int find_req_type(char *request);
void execute_request(int req_type, char *request, int client_fd, char *DNS, char *http_version);
void exec_get(); // TODO
void exec_head();
int correct_addr_form(char *addr);
char* concat(const char *s1, const char *s2);

char* concat(const char *s1, const char *s2)
{
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1); // +1 to copy the null-terminator
    return result;
}

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
        
        memset(recv_buffer, 0, sizeof(recv_buffer));
        if(recv(client_to_proxy, recv_buffer, sizeof(recv_buffer) - 1, 0) == -1) {
            err("receiving data from client");
        }
        printf("REQUEST: %s\n",recv_buffer);
        // variables to pass to the actual request function
        char *save_request = strdup(recv_buffer);
        char *token = strtok(save_request, " "); //We dont wanna modify the buffer with strtok before passing it to execute_request()
        int req_type;
        char *http_version = NULL;
        char *DNS = NULL;
        for (int i = 0; i < 3 && token; i++) {     
            // For the first token in the request, find out what kind of request it is (GET, POST, etc)
            if(i == 0) {
                printf("REQUEST TOKEN: {%s}\n", token);
                req_type = find_req_type(token);
                if(req_type == -1) {
                    printf("Incorrect request type\n");
                    break;
                }
            } else if(i == 1) {
                printf("DNS TOKEN: {%s}\n", token);
                DNS = token;
            } else if(i == 2) {
                http_version = token;
                printf("HTTP TOKEN: {%s}\n", token);
                if(HTTP_req(token)) {
                    printf("HTTP detected\n");
                } else {
                    printf("Not an HTTP request\n");
                    break;
                }
            }
            token = strtok(NULL, " ");
        }
        /* execute the request type */
        execute_request(req_type, recv_buffer, client_to_proxy, DNS, http_version);

        // NEED TO DELETE THIS BREAK EVENTUALLY
        break;
    }

    close(proxy_listener);
    return EXIT_SUCCESS;
}

// need to make this detect www. alone as well
int correct_addr_form(char *request) {
    printf("function: %s\n", request);
    char *ptr = NULL;
    if((ptr = strstr(request, HTTP_FRAME))) {
        printf("HTTP detected\n");
        return 0;
    }else{
        return 1;
    }
}

// TODO
void execute_request(int req_type, char *request, int client_fd, char *DNS, char *http_version) {
    printf("In execute_request()\n");
    /* gethostbyname() returns a struct hostent * */
    struct hostent *target_info = NULL;

    /* remote host address that sock_fd will connect to */
    struct sockaddr_in remote_host;

    /* socket that will connect to the address provided by target_info */
    int sock_fd;
    char *save_request = strdup(request);
    char send_buf[5000];
    memset(send_buf, 0, sizeof(send_buf));
            /* now that I have the DNS, I just need to retrieve the html string of that website, and then return it 
                to the client */
            printf("%s\n", DNS);
            int correct_addr = correct_addr_form(DNS);
            if(correct_addr == 0) {
                DNS = strtok(DNS,"/");
                DNS = strtok(NULL,"/");
                printf("DNS: %s\n", DNS);
            } else if(correct_addr == 1) {
                //Do we need to check for www frame?
                //DNS = strtok(DNS,"www.");
                //DNS = strtok(NULL,"www.");
            }
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
            
            http_version = strtok(http_version,"\r\n");
            printf("HTTP_VERSION: %s\n", http_version);
            /* setting up requestbase */
            char *requestbase = strstr(save_request, http_version); //Requestbase includes http version now
            printf("REQUEST BASE: %s\n", requestbase);
            if(!requestbase) {
                return; //prevent segfault
            }
            char *req = NULL;
            switch (req_type) {
                case GET:
                    req = concat("GET / ", requestbase);
                    //req = concat(req, "\r\n"); //We don't need another newline
                    break;
                case POST:
                    req = concat("POST / ", requestbase);
                    //req = concat(req, "\r\n");
                    break;
            }
            req = concat(req, "\r\n\r\n");
            printf("REQUEST: %s", req);
            if(send(sock_fd, req, strlen(req), 0) == -1) {
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
}

int find_req_type(char *request) {
    // + CONNECT method when we'll be using SSL/TLS
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
        case 'C':
            for(i = 1; isalpha(*(ptr + i)); i++) {
                //if(ptr[i] != CONNECT[i])
                    return -1;
            }
            //return CONNECT;
    }
    return -1;
}

int HTTP_req(char *request) {
    char *ptr = NULL;
    if((ptr = strstr(request, "HTTP/1.1")) || (ptr = strstr(request, "HTTP/1.0")))
        return 1;
    return 0;
}
