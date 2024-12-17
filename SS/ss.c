#include "headers.h"

 char naming_server_ip_backup [16]; // contains naming server IP 
# define BACK_SS_TO_NS_PORT "5113" // contains naming server port for backward communication
char* cwd ;
int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf(RED("Usage: %s <naming_server_ip> <naming_server_port>\n"), argv[0]);
        return 1;
    }

    StorageServer_init server;
    StorageServer_send send;
    memset(&server, 0, sizeof(StorageServer_init));
    memset(&send, 0, sizeof(StorageServer_send));
    server.unique_id = UNIQUE_ID;
    send.unique_id = UNIQUE_ID;
    strncpy(server.naming_server_ip, argv[1], sizeof(server.naming_server_ip) - 1);
    strcpy(naming_server_ip_backup, server.naming_server_ip);
    server.naming_server_port = atoi(argv[2]);
    char * buffer = (char *)malloc(16);
    get_local_ip(buffer);
    strcpy(server.local_ip, buffer);
    server.local_port = SS_PORT;
    strcpy(send.ss_ip, server.local_ip);
    send.ss_port = SS_PORT;
    send.backupport = BACKUP_PORT;

    // Create new directory for storage server files if it does not exist
    char new_dir[256];
    snprintf(new_dir, sizeof(new_dir), "SS_%d", UNIQUE_ID);
    struct stat st = {0};

    if (stat(new_dir, &st) == -1) {
        mkdir(new_dir, 0700);
    }
    if (chdir(new_dir) != 0) {
        perror("chdir");
        return 1;
    }
    cwd = getcwd(NULL, 0);
    char ** files = get_files(cwd, &send.num_files);
    printf(CYAN("%d\n"), send.num_files);
    int i=0;
    while(i<send.num_files){
        printf(CYAN("%s\n"), files[i]);
        i++;
    }
    send.stosport=S_SSPORT;

    int sock = register_with_naming_server(&server, &send, files);
    if (sock < 0) {
        printf(RED("Failed to register with naming server\n"));
        return 1;
    }

    // Create threads for naming server listener and client server handler
    pthread_t naming_server_thread, client_server_thread,storage_server_thread,backup_thread;

    if (pthread_create(&naming_server_thread, NULL, naming_server_listener, (void *)(uintptr_t)sock) != 0) {
        perror(RED("pthread_create for naming server listener"));
        return 1;
    }
    if (pthread_create(&storage_server_thread, NULL, storage_server_handler, NULL) != 0) {
        perror(RED("pthread_create for storage server listener"));
        return 1;
    }

    if (pthread_create(&client_server_thread, NULL, client_server_handler, &server) != 0) {
        perror(YELLOW("pthread_create for client server handler"));
        return 1;
    }

    if(pthread_create(&backup_thread,NULL,backup_server_handler,&send)!=0){
        perror(YELLOW("pthread_create for backup server handler"));
        return 1;
    }

    // Wait for threads to complete
    pthread_join(naming_server_thread, NULL);
    // pthread_join(client_server_thread, NULL);

    return 0;
}

void send_data_to_ns(async_ack * ack) {
    printf("Sending the data asynchornously");
    
    struct addrinfo hints, *res; // structure for getaddrinfo
    memset(&hints, 0 , sizeof hints);

    int ns_back_socket_fd;
    
    hints.ai_family = AF_UNSPEC; // can use either IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM; // we are using TCP sockets
    int status; 
    printf("Connected back successfully to port : %s\n", naming_server_ip_backup);
    // getting information about the server_ip and the server_port and using it to create out socket
    if ((status = getaddrinfo(naming_server_ip_backup, BACK_SS_TO_NS_PORT , &hints, &res)) != 0) {
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
    printf("Sending data to NS\n");
    send(ns_back_socket_fd, ack, sizeof(async_ack), 0);
        printf("Sent data to NS: %s\n", ack->data);
    close(ns_back_socket_fd);
}
