#include "headers.h"

#define BACK_CHANNEL_PORT 5113 // this is fixed : where the NS is listening 

char* get_local_ip(char* buffer) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Walk through linked list, finding the first non-localhost IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) {  // IPv4
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                           buffer, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s in SS/func.c\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            
            // Skip localhost
            if (strcmp(buffer, "127.0.0.1") != 0) {
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return buffer;
}

void send_data_to_client(async_ack * ack) {
    printf("Sending the data asynchornously from NS TO Client\n");
    
    struct addrinfo hints, *res; // structure for getaddrinfo
    memset(&hints, 0 , sizeof hints);

    int ns_back_socket_fd;
    
    hints.ai_family = AF_UNSPEC; // can use either IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM; // we are using TCP sockets
    int status; 
    char port_str[6]; // Enough to hold max port number "65535" and null terminator
    snprintf(port_str, sizeof(port_str), "%d", ack->async_port);
    printf("Connected back successfully to port : %d\n", ack->async_port);
    // getting information about the server_ip and the server_port and using it to create out socket
    if ((status = getaddrinfo(ack->client_ip, port_str , &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    ns_back_socket_fd = socket(res->ai_family , res->ai_socktype , res->ai_protocol); 

    if(connect(ns_back_socket_fd , res->ai_addr , res->ai_addrlen) == -1)
    {
        perror("connect failed");
        exit(1);
    }

    // Send data to NS
    printf("Sending data to client\n");
    send(ns_back_socket_fd, ack, sizeof(async_ack), 0);
    printf("Sent data to client: %s\n", ack->data);
    close(ns_back_socket_fd);
}

void * executessreq(void * arg)
{

    printf("Received teh data\n");
    int ss_soc=(int)(uintptr_t)arg;
    char buffer[200];
    int bytes_received = recv(ss_soc, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data
        printf("Received data from SS: %s\n", buffer);
        
        // Assuming buffer contains the ack structure, you need to cast it properly
        async_ack *ack = (async_ack *)buffer; // Cast buffer to async_ack type
        send_data_to_client(ack); // Send data to client using ack

    } else {
        perror("Receive failed");
    }
    return NULL;
}
void *handle_back_channel_communication(void *arg) {

    struct sockaddr_in ss_addr, server_addr;
    int server_fd;
    int ss_soc;
    socklen_t addrlen=sizeof(ss_addr);
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1){
        perror(YELLOW("Socket creation failed"));
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BACK_CHANNEL_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(&opt)) < 0){
        perror(YELLOW("set sockopt failed"));
    }
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror(YELLOW("Bind failed"));
    }
    if (listen(server_fd,MAXCLIENTS) < 0) {
        perror(YELLOW("Listen failed"));
    }
    ss_soc=-1;
    while(1){
        ss_soc=accept(server_fd,(struct sockaddr *)&ss_addr,&addrlen);
        if (ss_soc==-1) {
            perror(YELLOW("Error while accepting connection"));
        }
        pthread_t handle;
        pthread_create(&handle,NULL,&executessreq,(void *)(uintptr_t)ss_soc);
    }
    close(server_fd);
    return NULL;
}