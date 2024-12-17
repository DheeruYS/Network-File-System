#include "headers.h"
#include <pthread.h>
// 1 : READ
// 2 : WRITE 
// 4 : RETRIEVE
// 5 : STREAM
void *handle_client(void *arg) {
    while(1){
        int client_soc=(int)(uintptr_t)arg;
        // printf(PINK("%d\n"),client_soc);
        // fflush(stdout);
        incomingreqclient *b=(incomingreqclient *)malloc(sizeof(incomingreqclient)); // user request has been stored in b 
        int temp=recv(client_soc,b,sizeof(incomingreqclient),0);
        if (temp <= 0) {  // Check for client disconnect or error
            if (temp == 0) {
                printf(YELLOW("Client disconnected\n"));
            } else {
                perror(YELLOW("recv error"));
            }
            free(b);
            close(client_soc);
            break;
        }
        clientrequest *a=(clientrequest *)malloc(sizeof(clientrequest));
        a->request=b->type;
        a->socket=client_soc;
        // printf("%s", b->path);
        strcpy(a->path,b->path);
        strcpy(a->dest,b->dest);
        strcpy(a->file,b->file);
        char ack_ip[16]; // getting some additinal information about the user
        strcpy(ack_ip , b->client_ip);
        int back_port = b->async_port;
        free(b);

        // printing just for checking
        printf(PINK("socket: %d \nrequest: %d \npath: %s \ndest: %s \nfile: %s \n"),a->socket,a->request,a->path,a->dest,a->file);
        // 4 kind of request can be there
        // based on the task we need to send the response
        if(a->request == 1) // it is a read request
        {
            read_fn(a);
        }
        else if(a->request == 10) // it is a aysnc write request
        {
            printf("async write received\n");
            int timeout = 100; // Timeout in seconds
            // Start the timer
            clock_t start_time = clock();
            if (async_write_fn(a, 0) == 0) {
                clock_t end_time = clock();
                double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
                if (elapsed_time < timeout) {
                    async_ack ack_asyn;
                    ack_asyn.async_port = back_port;
                    strcpy(ack_asyn.client_ip, ack_ip);
                    strcpy(ack_asyn.data , "Async Ack : Successfully complete Write operation\n");
                    send_data_to_ns(&ack_asyn);
                } 
                else {
                    printf(RED("Async write took too long \n"));
                }
            } 
            else {
                printf(RED("async_write_fn failed.\n"));
            }
        }
        else if(a->request == 2) // it is a write request
        {
            write_fn(a, 0);
        }
        else if(a->request == 9) // it is an append request
        {
            write_fn(a, 1);
        }
        else if(a->request == 4) // it is a retrieve request
        {
            char command[1024];
            char buffer[1024];
            FILE *fp;

            strcpy(command, "ls -l ");
            strcat(command, cwd);
            strcat(command, "/");
            strcat(command, a->path);

            fp = popen(command, "r");
            if (fp == NULL) {
                perror("popen");
                char error_msg[] = "Error executing command";
                send(a->socket, error_msg, strlen(error_msg), 0);
            } else {
                while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                    printf("%s\n",buffer);
                    send(a->socket, buffer, strlen(buffer), 0);
                }
                pclose(fp);
            }
        }
        else if(a->request == 5) // it is a stream request
        {
            char filepath[1024];
            snprintf(filepath, sizeof(filepath), "%s/%s", cwd, a->path);
            FILE *file = fopen(filepath, "rb");
            if (file == NULL) {
                perror("fopen");
                char buffer[] = "Error opening file";
                send(a->socket, buffer, strlen(buffer), 0);
            } else {
                char buffer[1024];
                size_t bytes_read;
                while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                    send(a->socket, buffer, bytes_read, 0);
                }
                fclose(file);
            }
        }
        else 
        {
            // invalid request and send the response   
            char buffer[]="invalid request type";
            send(a->socket,buffer,strlen(buffer),0);
        }
        // char buffer[]="hi hello";
        // send(a->socket,buffer,strlen(buffer),0);
        free(a);
        close(client_soc);
        break;
    }
    return NULL;
}

void *client_server_handler(void *arg) {
    StorageServer_init *server = (StorageServer_init *)arg;

    // Create server socket to listen for clients
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror(YELLOW("socket"));
        return NULL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server->local_port);

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror(YELLOW("set sockopt failed"));
    }

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror(YELLOW("bind"));
        close(server_socket);
        return NULL;
    }

    if (listen(server_socket, 5) < 0) {
        perror(YELLOW("listen"));
        close(server_socket);
        return NULL;
    }

    printf(PINK("Waiting for clients to connect...\n"));

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror(YELLOW("accept"));
            continue;
        }

        printf("GREEN(Client connected)\n");

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client,(void *)(uintptr_t) client_socket) != 0) {
            perror(YELLOW("pthread_create"));
            // free(client_socket);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_socket);
    return NULL;
}
