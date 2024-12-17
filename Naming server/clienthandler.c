#include "headers.h"
void *clientreqs(void *arg){
    struct sockaddr_in client_addr,server_addr;
    int server_fd;
    int client_soc;
    socklen_t addrlen=sizeof(client_addr);
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd==-1){
        perror(YELLOW("Socket creation failed"));
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NS_PORT);
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
    client_soc=-1;
    while(1){
        client_soc=accept(server_fd,(struct sockaddr *)&client_addr,&addrlen);
        if (client_soc==-1) {
            perror(YELLOW("Error while accepting connection"));
        }
        pthread_t handle;
        pthread_create(&handle,NULL,&executeclient,(void *)(uintptr_t)client_soc);
    }
    close(server_fd);
    return NULL;
}
void *executeclient(void *arg){
    while(1){
        int client_soc=(int)(uintptr_t)arg;
        incomingreqclient *b=(incomingreqclient *)malloc(sizeof(incomingreqclient));
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
        a->ping=0;
        a->socket=client_soc;
        a->kind=b->kind;
        strcpy(a->path,b->path);
        strcpy(a->dest,b->dest);
        strcpy(a->file,b->file);
        free(b);
        logs(0, 0, client_soc, a->request, "Request Received", SUCCESS);
        // printf("socket: %d \nrequest: %d \npath: %s \ndest: %s \nfile: %s \nkind: %d \n",a->socket,a->request,a->path,a->dest,a->file,a->kind);
        // char buffer[]="hi hello";
        // send(a->socket,buffer,strlen(buffer),0);
        printsslist();
        if(a->kind==1){
            getipandport(a);
        }
        else if(a->kind==2){
            // displayAllStrings(root);

            if(a->request == 3) // delete
            {
                delete_file(a);
            }
            else if(a->request == 6) //create 
            {
                create_file(a);
            }
            else if(a->request==7){
                copyfile(a);
            }
            // pingserver(a);
        }
        else if(a->kind == 3){
            calllist(a);
        }
        free(a);
    }
    return NULL;
}
void copyfile(clientrequest *a){
    kind2Ack tosend;
    TrieNode* source = get_from_cache(a->path);

    printf("FILE NAME%s\n",a->path);

    if(source == NULL){
        printf(RED("Source path not found\n"));
        strcpy(tosend.ack_message,"Source path not found\n");
        tosend.errorCode=FILE_NOT_FOUND;
        send(a->socket,&tosend,sizeof(tosend),0);
        logs(0, 0, a->socket, a->request, "Source path not found", FILE_NOT_FOUND);
        return;
    }
    TrieNode* dest = get_from_cache(a->dest);
    if(dest == NULL){
        printf(RED("Destination path not found\n"));
        strcpy(tosend.ack_message,"Destination path not found\n");
        tosend.errorCode=FILE_NOT_FOUND;
        send(a->socket,&tosend,sizeof(tosend),0);
        logs(0, 0, a->socket, a->request, "Destination path not found", FILE_NOT_FOUND);
        return;
    }

    if(!strcmp(a->path,".")) source->storageList = find_server_with_least_files();
    if(!strcmp(a->dest,".")) dest->storageList = find_server_with_least_files();

    printf("source: %s %d\ndest: %s %d\n",source->storageList->server_ip,source->storageList->unique_id,dest->storageList->server_ip,dest->storageList->unique_id);
    printf(PINK("Sending files from %d to %d\n"), source->storageList->unique_id, dest->storageList->unique_id);
    clientrequest* req = (clientrequest*)malloc(sizeof(clientrequest));
    strcpy(req->path, a->path);
    strcpy(req->dest, a->dest);
    strcpy(req->server_ip,dest->storageList->server_ip);
    req->port=dest->storageList->server_port;
    req->request=7;
    req->ping=0;
    req->stosport=dest->storageList->stosport;

    // send_pack_to_sserver(source->storageList->socket, req);

    send(source->storageList->socket, req, sizeof(clientrequest), 0);
    logs(0, 0, a->socket, a->request, "File copy request sent", SUCCESS);
    recv(source->storageList->socket,req,sizeof(clientrequest),0);
    if(req->ping==2){
        strcpy(tosend.ack_message,"Copied successfully.\n");
        copyFile(root, a->path, a->dest);
    }
    else if(req->ping==3){
        strcpy(tosend.ack_message,"Destination is not a valid directory.\n");
    }
    send(a->socket,&tosend,sizeof(tosend),0);   
}

void getipandport(clientrequest *a) {
    char full_path[MAXPATHLEN + MAXFILENAME];
    if (strlen(a->file) > 0) {
        snprintf(full_path, sizeof(full_path), "%s/%s", a->path, a->file);
    } else {
        strcpy(full_path, a->path);
    }

    kind1Response tosend;
    TrieNode *response = get_from_cache(a->path);
    if (response == NULL) {
        strcpy(tosend.storageServerIP, "");
        tosend.storageServerPort = -1;
        tosend.errorCode = FILE_NOT_FOUND;
        send(a->socket, &tosend, sizeof(tosend), 0);
        logs(0, 0, a->socket, a->request, "Path not found", FILE_NOT_FOUND);
        recv(a->socket, &tosend, sizeof(tosend), 0);
        printf("ACK got\n");
        return;
    }
    int check=0;
    if(a->request==1||a->request==5||a->request==4){
        if(getfilereadlock(response)==0){
            strcpy(tosend.storageServerIP, "");
            tosend.storageServerPort = -1;
            tosend.errorCode = TIMEOUT;
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, "Timeout", STORAGE_SERVER_NOT_FOUND);
            check=1;
            recv(a->socket, &tosend, sizeof(tosend), 0);
            return;
        }
    }
    else{
        if(getfilewritelock(response)==0){
            strcpy(tosend.storageServerIP, "");
            tosend.storageServerPort = -1;
            tosend.errorCode = TIMEOUT;
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, "Timeout", STORAGE_SERVER_NOT_FOUND);
            check=1;
            recv(a->socket, &tosend, sizeof(tosend), 0);
            return;
        }
    }

    sserverlist backup1 = response->storageList->backup1;
    sserverlist backup2 = response->storageList->backup2;

    if (response->storageList != NULL && pingserver(response->storageList->socket)) {
        strcpy(tosend.storageServerIP, response->storageList->server_ip);
        tosend.storageServerPort = response->storageList->server_port;
        tosend.errorCode = SUCCESS;
        strcpy(tosend.fileName, a->path);
        tosend.unique_id = response->storageList->unique_id;

        if(backup1){
            strcpy(tosend.backup1ServerIP, backup1->server_ip);
            tosend.backup1ServerPort = backup1->server_port;
        }
        else{
            strcpy(tosend.backup1ServerIP, "");
            tosend.backup1ServerPort = -1;
        }

        if(backup2){
            strcpy(tosend.backup2ServerIP, backup2->server_ip);
            tosend.backup2ServerPort = backup2->server_port;
        }
        else{
            strcpy(tosend.backup2ServerIP, "");
            tosend.backup2ServerPort = -1;
        }

        printf("Debug %d %s" , tosend.storageServerPort, tosend.storageServerIP);
        send(a->socket, &tosend, sizeof(tosend), 0);
        logs(0, 0, a->socket, a->request, "Primary storage server found", SUCCESS);
    } else if (backup1 != NULL && pingserver(backup1->socket) && (a->request==1 || a->request==5 || a->request==4)) {
        strcpy(tosend.storageServerIP, backup1->server_ip);
        tosend.storageServerPort = backup1->server_port;
        tosend.errorCode = SUCCESS;
        snprintf(tosend.fileName, sizeof(tosend.fileName), "BACKUP_SS%d/./%s",response->storageList->unique_id, a->path);
        send(a->socket, &tosend, sizeof(tosend), 0);
        logs(0, 0, a->socket, a->request, "Backup storage server found", SUCCESS);
    }else if (backup2 != NULL && pingserver(backup2->socket) && (a->request==1 || a->request==5 || a->request==4)) {
        strcpy(tosend.storageServerIP, backup2->server_ip);
        tosend.storageServerPort = backup2->server_port;
        tosend.errorCode = SUCCESS;
        snprintf(tosend.fileName, sizeof(tosend.fileName), "BACKUP_SS%d/./%s",response->storageList->unique_id, a->path);
        send(a->socket, &tosend, sizeof(tosend), 0);
        logs(0, 0, a->socket, a->request, "Backup storage server found", SUCCESS);
    } 
    else {
        strcpy(tosend.storageServerIP, "");
        tosend.storageServerPort = -1;
        tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
        send(a->socket, &tosend, sizeof(tosend), 0);
        logs(0, 0, a->socket, a->request, "No storage server found", STORAGE_SERVER_NOT_FOUND);
    }
    recv(a->socket, &tosend, sizeof(tosend), 0);
    printf("ACK got\n");
    // if(check==0){
        // printf("here");
        if(a->request==1||a->request==5||a->request==4){
            releasefilereadlock(response);
        }
        else{
            releasefilewritelock(response);
        }
    // }
}

// Function to find the server with the least number of files
sserverlist find_server_with_least_files() {
    getssreadlock();
    sserverlist head = sshead;
    sserverlist least_server = NULL;
    int min_files = INT_MAX;

    while (head != NULL) {
        if (head->num_files < min_files) {
            if(pingserver(head->socket)==1){
                min_files = head->num_files;
                least_server = head;
            }
        }
        head = head->next;
    }
    releasessreadlock();
    return least_server;
}

void create_file(clientrequest *a){

    int final_unique_id;

    if (strlen(a->file) == 0) {
        // Extract the first folder from the path
        char first_folder[MAXFILENAME];
        char *slash_pos = strchr(a->path, '/');
        if (slash_pos != NULL) {
            size_t length = slash_pos - a->path;
            strncpy(first_folder, a->path, length);
            first_folder[length] = '\0'; // Null-terminate the string
        } else {
            strcpy(first_folder, a->path); // No slash found, take the whole path
        }

        kind2Ack tosend;

        if(strcmp(first_folder,".") == 0){
            strcpy(tosend.ack_message, "Failed to process folder\n");
            tosend.errorCode = ACCESS_DENIED;
            send(a->socket, &tosend, sizeof(tosend), ACCESS_DENIED);
            logs(0, 0, a->socket, a->request, "Failed to process folder", INVALID_PATH);
            return;
        }

        TrieNode *response = get_from_cache(first_folder);

        if (response != NULL) {

            TrieNode *check = get_from_cache(a->path);
            if(check != NULL){
                strcpy(tosend.ack_message, "File already exists\n");
                tosend.errorCode = FILE_ALREADY_EXISTS;
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, "File already exists", FILE_ALREADY_EXISTS);
                return;
            }

            // Folder exists, send to the storage server
            int x=pingserver(response->storageList->socket);
            if(x==0){
                strcpy(tosend.ack_message, "Server down.\n");
                tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, "Server down", INVALID_PATH);
                return;
            }
            int status = send_pack_to_sserver(response->storageList->socket, a);
            if (status) {
                strcpy(tosend.ack_message, "Folder processed successfully\n");
                tosend.errorCode = SUCCESS;

                char temp[MAXPATHLEN];
                strcpy(temp, a->path);
                snprintf(a->path, sizeof(a->path), "BACKUP_SS%d/./%s",response->storageList->unique_id, temp);
                a->ping = 0;
                if(response->storageList->backup1){
                    status = send_pack_to_sserver(response->storageList->backup1->socket, a);
                    if (status) printf("Backup 1 updated\n");
                    else printf("Backup 1 failed\n");
                } 
                a->ping = 0;
                if(response->storageList->backup2){
                    status = send_pack_to_sserver(response->storageList->backup2->socket, a);
                    if (status) printf("Backup 2 updated\n");
                    else printf("Backup 2 failed\n");
                }
                strcpy(a->path,temp);
            } else {
                strcpy(tosend.ack_message, "Failed to process folder\n");
                tosend.errorCode = FILE_ALREADY_EXISTS;
            }
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
            final_unique_id = response->storageList->unique_id;
        } else {
            // Folder does not exist, find the server with the least files
            sserverlist least_server = find_server_with_least_files();
            if (least_server != NULL) {
                if(pingserver(least_server->socket)==0){
                    strcpy(tosend.ack_message, "Server down.\n");
                    tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
                    send(a->socket, &tosend, sizeof(tosend), 0);
                    logs(0, 0, a->socket, a->request, "Server down", INVALID_PATH);
                    return;
                }
                // Create a new node for the server and send the request
                // Assuming you have a function to create a new folder on the server
                int status = send_pack_to_sserver(least_server->socket, a);
                if (status) {
                    strcpy(tosend.ack_message, "Folder created successfully on least loaded server\n");
                    tosend.errorCode = SUCCESS;

                    char temp[MAXPATHLEN];
                    strcpy(temp, a->path);
                    snprintf(a->path, sizeof(a->path), "BACKUP_SS%d/./%s",response->storageList->unique_id, temp);
                    a->ping = 0;
                    if(response->storageList->backup1){
                        status = send_pack_to_sserver(response->storageList->backup1->socket, a);
                        if (status) printf("Backup 1 updated\n");
                        else printf("Backup 1 failed\n");
                    } 
                    a->ping = 0;
                    if(response->storageList->backup2){
                        status = send_pack_to_sserver(response->storageList->backup2->socket, a);
                        if (status) printf("Backup 2 updated\n");
                        else printf("Backup 2 failed\n");
                    }
                    strcpy(a->path,temp);
                } else {
                    strcpy(tosend.ack_message, "Failed to create folder on server\n");
                    tosend.errorCode = FILE_NOT_CREATED;
                }
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
                final_unique_id = least_server->unique_id;
            } else {
                strcpy(tosend.ack_message, "No available storage servers\n");
                tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
            }
        }
    } 
    else {
        TrieNode *response =get_from_cache(a->path);
        kind2Ack tosend;
        if (response == NULL) {
            strcpy(tosend.ack_message, "Path Not Found\n");
            tosend.errorCode = FILE_NOT_FOUND;
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
            return;
        }
        
        char temp[MAXPATHLEN + MAXFILENAME];

        if (strcmp(a->path, ".")) snprintf(temp, sizeof(temp), "%s/%s", a->path, a->file);
        else strcpy(temp, a->file);

        printf(PINK("%s\n"), temp);
        printf(PINK("%s/%s\n"), a->path, a->file);
        TrieNode *check =get_from_cache(temp);

        if (check != NULL) {
            strcpy(tosend.ack_message, "File already exists\n");
            tosend.errorCode = FILE_ALREADY_EXISTS;
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
            return;
        }

        int status;
        if (!strcmp(a->path, ".")) {
            sserverlist least_server = find_server_with_least_files();
            if (least_server == NULL) {
                strcpy(tosend.ack_message, "No available storage servers\n");
                tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
                return;
            }
            if(pingserver(least_server->socket)==0){
                strcpy(tosend.ack_message, "Server down.\n");
                tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, "Server down", INVALID_PATH);
                return;
            }
            status = send_pack_to_sserver(least_server->socket, a);
            final_unique_id = least_server->unique_id;
        } else {
            if(pingserver(response->storageList->socket)==0){
                strcpy(tosend.ack_message, "Server down.\n");
                tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
                send(a->socket, &tosend, sizeof(tosend), 0);
                logs(0, 0, a->socket, a->request, "Server down", INVALID_PATH);
                return;
            }
            status = send_pack_to_sserver(response->storageList->socket, a);
            final_unique_id = response->storageList->unique_id;
        }

        if (status) {
            strcpy(tosend.ack_message, "File Updated\n");
            tosend.errorCode = SUCCESS;

            char temp[MAXPATHLEN];
            strcpy(temp, a->path);
            snprintf(a->path, sizeof(a->path), "BACKUP_SS%d/./%s",response->storageList->unique_id, temp);
            a->ping = 0;
            if(response->storageList->backup1){
                status = send_pack_to_sserver(response->storageList->backup1->socket, a);
                if (status) printf("Backup 1 updated\n");
                else printf("Backup 1 failed\n");
            } 
            a->ping = 0;
            if(response->storageList->backup2){
                status = send_pack_to_sserver(response->storageList->backup2->socket, a);
                if (status) printf("Backup 2 updated\n");
                else printf("Backup 2 failed\n");
            }
            strcpy(a->path,temp);         
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
        } else {
            strcpy(tosend.ack_message, "File not Updated\n");
            tosend.errorCode = FILE_NOT_CREATED;
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
        }
    }

    char path_copy[MAXPATHLEN];
    strncpy(path_copy, a->path, MAXPATHLEN);
    
    char *token = strtok(path_copy, "/");
    char full_path[MAXPATHLEN] = ""; // To build the full path incrementally

    while (token != NULL) {
        // Append the current folder to the full path
        if (strlen(full_path) > 0) {
            strcat(full_path, "/"); // Add a slash if it's not the first folder
        }
        strcat(full_path, token);

        printf("%s\n", full_path);   

        // Insert the current folder into the Trie
        build_trie(root, full_path, final_unique_id);

        printf("%d\n", get_from_cache(full_path)->isEndOfWord);

        // Move to the next folder
        token = strtok(NULL, "/");
    }

    // If a file is specified, insert it as well
    if (strlen(a->file) > 0) {
        if (strcmp(a->path, ".")) strcat(full_path, "/"); // Add a slash before the file name
        strcat(full_path, a->file);
        build_trie(root, full_path, final_unique_id);
    }
}

void delete_file(clientrequest *a){
    TrieNode *response=get_from_cache(a->path);
    kind2Ack tosend;

    if(response==NULL){
        strcpy(tosend.ack_message,"Path Not Found\n");
        tosend.errorCode=FILE_NOT_FOUND;
        send(a->socket,&tosend,sizeof(tosend),0);
        logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
        return;
    }
    else{
        char temp[MAXPATHLEN+MAXFILENAME];
        if(strlen(a->file) > 0){
            if(strcmp(a->path,".")) snprintf(temp, sizeof(temp), "%s/%s", a->path, a->file);
            else snprintf(temp, sizeof(temp), "%s%s", a->path, a->file);
        }
        else{
            strcpy(temp,a->path);
            if(strcmp(a->path,".") == 0){
                strcpy(tosend.ack_message,"Operation Not Allowed\n");
                tosend.errorCode = ACCESS_DENIED;
                send(a->socket,&tosend,sizeof(tosend),0);
                logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
                return;
            }
        }

        printf(PINK("%s\n"),temp);
        printf(PINK("%s/%s\n"),a->path,a->file);
        
        TrieNode *check=get_from_cache(temp);
        // printf(RED("%d\n"),check->isEndOfWord);
        if(check==NULL){
            strcpy(tosend.ack_message,"File does not exist\n");
            tosend.errorCode = FILE_NOT_FOUND;
            send(a->socket,&tosend,sizeof(tosend),0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
            return;
        }
        if(pingserver(response->storageList->socket)==0){
            strcpy(tosend.ack_message, "Server down.\n");
            tosend.errorCode = STORAGE_SERVER_NOT_FOUND;
            send(a->socket, &tosend, sizeof(tosend), 0);
            logs(0, 0, a->socket, a->request, "Server down", INVALID_PATH);
            return;
        }
        int status = send_pack_to_sserver(check->storageList->socket,a);
        if(status){
            deleteFile(root,temp);
            strcpy(tosend.ack_message,"File Deleted\n");
            tosend.errorCode=SUCCESS;
            
            char temp[MAXPATHLEN];
            strcpy(temp, a->path);
            snprintf(a->path, sizeof(a->path), "BACKUP_SS%d/./%s",response->storageList->unique_id, temp);
            a->ping = 0;
            if(response->storageList->backup1){
                status = send_pack_to_sserver(response->storageList->backup1->socket, a);
                if (status) printf("Backup 1 updated\n");
                else printf("Backup 1 failed\n");
            } 
            a->ping = 0;
            if(response->storageList->backup2){
                status = send_pack_to_sserver(response->storageList->backup2->socket, a);
                if (status) printf("Backup 2 updated\n");
                else printf("Backup 2 failed\n");
            }
            strcpy(a->path,temp);
            
            send(a->socket,&tosend,sizeof(tosend),0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
        }
        else{
            strcpy(tosend.ack_message,"File not Deleted\n");
            tosend.errorCode=FILE_NOT_DELETED;
            send(a->socket,&tosend,sizeof(tosend),0);
            logs(0, 0, a->socket, a->request, tosend.ack_message, tosend.errorCode);
        }
    }
}

void calllist(clientrequest *a) {
    char buffer[MAXPATHLEN]={'\0'};
    buffer[0] = '\0'; // Initialize buffer

    if (strcmp(a->path, "~") == 0) {
        // Print all files
        gettriereadlock();
        listAccessiblePaths(root, buffer, 0, a->socket, 1);
        releasetriereadlock();
    } else {
        // Print directories and files in the specified directory
        TrieNode *dirNode = get_from_cache(a->path);
        if(dirNode){
            dirNode = dirNode->children[47];
        }
        else{
            char errorMsg[] = "Directory not found\n";
            send(a->socket, errorMsg, strlen(errorMsg), 0);
            logs(0, 0, a->socket, a->request, "Directory not found", FILE_NOT_FOUND);
        }

        if (dirNode) {
            gettriereadlock();
            listAccessiblePaths(dirNode, buffer, 0, a->socket, 1);
            releasetriereadlock();
        }else {
            char errorMsg[] = "No Files Found\n";
            send(a->socket, errorMsg, strlen(errorMsg), 0);
            logs(0, 0, a->socket, a->request, "No Files Found", FILE_NOT_FOUND);
        }
    }
    // memset(buffer,0,MAXPATHLEN);
    // strcpy(buffer,"");
    // send(a->socket,buffer,MAXPATHLEN,0);
    // printf("\n\n");
    char endSignal[] = "No More files / directories found\n\0";

    if (send(a->socket, endSignal,  strlen(endSignal) , 0) < 0) {
        perror("Error sending end-of-transmission signal");
    }
    logs(0, 0, a->socket, a->request, "Finished sending data", SUCCESS);
    printf("Finished sending data.\n");
}