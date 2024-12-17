#include "headers.h"
#include <pthread.h>

void *naming_server_handle_request(nsrequest* message, int sock) {
    printf(BLUE("Received request from client\n"));
    printf(BLUE("Ping %d\n"), message->ping);
    printf(BLUE("Request type: %d\n"), message->request);
    printf(BLUE("Path: %s\n"), message->path);
    printf(BLUE("Destination: %s\n"), message->dest);
    printf(BLUE("File: %s\n"), message->file);
    // send(sock, message, sizeof(nsrequest), 0);

    if(message->ping == 1) {
        printf(GREEN("Ping request received\n"));
        send(sock, message, sizeof(nsrequest), 0);
        printf(GREEN("Server is up\n"));
    }
    else if(message->request == 6) {
        printf(GREEN("Create request received\n"));
        
        char full_path[MAXPATHLEN + MAXFILENAME];
        snprintf(full_path, sizeof(full_path), "%s/%s", message->path, message->file);

        // Create the directory if it does not exist
        struct stat st;
        if (stat(message->path, &st) != 0) {
            // Directory does not exist, attempt to create it
            char command[MAXPATHLEN + 20]; // Extra space for "mkdir -p " and null terminator
            snprintf(command, sizeof(command), "mkdir -p \"%s\"", message->path);
            // Execute the command
            if (system(command) != 0) {
                perror("Failed to create directory");
                return NULL; // Exit if there's an error
            }
        }

        if(strlen(message->file)){
            // Create the file
            int fd = open(full_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) {
                perror("Failed to create file");
                return NULL;
            }

            close(fd);
            printf(GREEN("File created successfully: %s\n"), full_path);
        }

        message->ping = 1;
        send(sock, message, sizeof(nsrequest), 0);
    }
    else if (message->request == 3) {
        printf(GREEN("Delete request received\n"));

        if (strlen(message->file) == 0) {
            // If file name is empty, delete the directory
            char command[MAXPATHLEN + 10];
            snprintf(command, sizeof(command), "rm -r %s", message->path);
            int result = system(command);
            if (result == 0) {
                printf(GREEN("Directory deleted successfully: %s\n"), message->path);
            } else {
                perror(RED("Failed to delete directory"));
                return NULL;
            }
        } else {
            // If file name is not empty, attempt to delete the file using rmdir
            // This will only work if the file is actually a directory
            char full_path[MAXPATHLEN + MAXFILENAME];
            snprintf(full_path, sizeof(full_path), "%s/%s", message->path, message->file);

            if (unlink(full_path) == 0) {
                printf(GREEN("File deleted successfully: %s\n"), full_path);
            } else {
                perror(RED("Failed to delete file"));
                return NULL;
            }
        }

        message->ping = 1;
        send(sock, message, sizeof(nsrequest), 0);
    }
    else if(message->request==7){
        // printf("%s %d %d\n",message->server_ip,message->port,message->stosport);
        pthread_t sserver;
        pthread_create(&sserver,NULL,storage_connect,(void *)message);
        pthread_join(sserver,NULL);
        if(message->ping==2){
            //success
            send(sock,message,sizeof(nsrequest),0);
        }
        else{
            //failed
            send(sock,message,sizeof(nsrequest),0);
        }
        //copy
    }
    else if(message->request == 8){
        printf("COPY REQUEST\n");
        printf("%s %s\n", message->path, message->dest);
        pthread_t sserver;
        pthread_create(&sserver,NULL,backup_connect,(void *)message);
        pthread_join(sserver,NULL);
        if(message->ping==2){
            //success
            send(sock,message,sizeof(nsrequest),0);
        }
        else{
            //failed
            send(sock,message,sizeof(nsrequest),0);
        }
    }

    return NULL;
}

void *naming_server_listener(void *arg) {
    
    int ns_soc=(int)(uintptr_t)arg;

    printf(GREEN("Listening for naming server requests...\n"));

    // Continuously accept requests from naming server
    while (1) {
        nsrequest *b=(nsrequest *)malloc(sizeof(nsrequest));
        int temp=recv(ns_soc,b,sizeof(nsrequest),0);
        if (temp <= 0) {  // Check for client disconnect or error
            if (temp == 0) {
                printf(RED("Naming Server Disconnected\n"));
            } else {
                perror(RED("recv error here"));
            }
            close(ns_soc);
            break;
        }
        naming_server_handle_request(b,ns_soc);
    }

    close(ns_soc);
    return NULL;
}