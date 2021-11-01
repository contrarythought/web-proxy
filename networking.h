#ifndef NETWORK_H
#define NETWORK_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h> /* for looking up hosts via DNS (struct hostent, gethostbyname()) */

#define WEB_PORT 80

enum REQUEST_TYPE {
    GET = 1,
    POST,
    PUT
};

void dump(char *str) {
    printf("%s", str);
    //printf("\n");
    int i;
    for(i = 0; i < strlen(str); i++) {
        printf("%02x ", str[i]);
    }
    printf("\n");
}

#endif