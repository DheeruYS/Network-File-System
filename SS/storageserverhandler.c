#include "headers.h"
void *storage_server_handler(void *arg){
    struct sockaddr_in ss_addr,server_addr;
    int server_fd;
    int server_soc;
    socklen_t addrlen=sizeof(ss_addr);
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1){
        perror(YELLOW("Socket creation failed"));
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(S_SSPORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror(YELLOW("set sockopt failed"));
    }
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror(YELLOW("Bind failed"));
    }
    if (listen(server_fd,MAXSSERVER) < 0) {
        perror(YELLOW("Listen failed"));
    }
    server_soc=-1;
    while(1){
        server_soc=accept(server_fd,(struct sockaddr *)&ss_addr,&addrlen);
        if (server_soc==-1) {
            perror(YELLOW("Error while accepting connection"));
        }
        pthread_t handle;
        pthread_create(&handle,NULL,&executeserver,(void *)(uintptr_t)server_soc);
    }
    close(server_fd);
    return NULL;
}
void *executeserver(void *arg){
    int ssocket=(int)(uintptr_t)arg;        //stospart dest
    printf(GREEN("Server is transfering the files.\n"));
    ss_to_client_ack *ab=(ss_to_client_ack *)malloc(sizeof(ss_to_client_ack));
    recv(ssocket,ab,sizeof(ss_to_client_ack),0);
    int i=ab->errorCode;
    int j=checkPath(ab->ack_message);
    if(j!=1){
        printf("Invalid dest.\n");
        send(ssocket,&j,sizeof(int),0);
        return NULL;
    }
    send(ssocket,&j,sizeof(int),0);
    if(i==-1){
        printf(RED("Error in source.\n"));
        return NULL;
    }
    if(i==0){
        receive_file(ssocket,ab->ack_message);
    }
    else if(i==1){
        receive_directory(ssocket,ab->ack_message);
    }
    return NULL;
}
int checkPath(char *path) {
    struct stat pathStat;
    
    if (stat(path, &pathStat) != 0) {
        perror("stat");
        return -1;
    }

    if (S_ISREG(pathStat.st_mode)) {
        return 0;
    } else if (S_ISDIR(pathStat.st_mode)) {
        return 1;
    } else {
        return -1;
    }
}
void *storage_connect(void *arg){
    //connect to ss ,arg is message(nsrequest*) with ip and stosport
    nsrequest *a=(nsrequest *)(arg);
    printf("destip: %s\ndestport: %d\ndestpath: %s\nsourcefile: %s\n",a->server_ip,a->stosport,a->dest,a->path);
    struct addrinfo hints, *res;
    int storage_socket_fd;
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", a->stosport);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status = getaddrinfo(a->server_ip, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return NULL;
    }

    storage_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(storage_socket_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect to storage failed");
        freeaddrinfo(res);
        return NULL;
    }
    freeaddrinfo(res);
    printf(GREEN("Server connected to destination server for file transfer.\n"));   //source
    int i=checkPath(a->path);
    ss_to_client_ack *ab=(ss_to_client_ack *)malloc(sizeof(ss_to_client_ack));
    ab->errorCode=i;
    strcpy(ab->ack_message,a->dest);
    send(storage_socket_fd,ab,sizeof(ss_to_client_ack),0);
    printf("sent");
    int g=0;
    recv(storage_socket_fd,&g,sizeof(int),0);
    if(g==1){
        if(i==0){
            send_file(a->path,storage_socket_fd);
        }
        else if(i==1){
            char buffer[MAXFILENAME];
            send_directory(a->path,storage_socket_fd,"");
            send_directoryfiles(a->path,storage_socket_fd,"");
            strcpy(buffer,"EOD");
            send(storage_socket_fd, buffer, MAXFILENAME, 0);
        }
        a->ping=2;
    }
    else{
        a->ping=3;
    }
    return NULL;
}