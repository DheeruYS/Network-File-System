#include "headers.h"

void printsslist(){
    getssreadlock();
    if(sshead==NULL){
        releasessreadlock();
        return;
    }
    sserverlist temp=sshead;
    while(temp!=NULL){
        printf("%s %d %d %d\n",temp->server_ip,temp->server_port,temp->unique_id,temp->num_files);
        temp=temp->next;
    }
    releasessreadlock();
}
sserverlist createnode(){
    sserverlist temp=(sserverlist)malloc(sizeof(storagelist));
    temp->unique_id=-1;
    strcpy(temp->server_ip,"");
    temp->server_port=-1;
    temp->next=NULL;
    temp->num_files=0;
    temp->socket=-1;
    temp->stosport=-1;
    temp->backupport=-1;
    return temp;
}

int create_backup_folder(int server_id, const char* server_ip, int port, int socket, int stosport) {
    clientrequest *req = (clientrequest *)malloc(sizeof(clientrequest));
    req->socket = socket;
    req->request = 6;
    req->ping = 0;
    snprintf(req->path, MAXPATHLEN, "BACKUP_SS%d", server_id);
    strncpy(req->server_ip, server_ip, sizeof(req->server_ip) - 1);
    req->kind = 2;
    req->port = port;
    req->stosport = stosport;
    
    int x = send_pack_to_sserver(socket, req);
    free(req);
    return x;
}

int copy_all_files(int source_socket, const char* dest_server_ip, int dest_server_port, int unique_id){
    clientrequest *req = (clientrequest *)malloc(sizeof(clientrequest));
    req->socket = source_socket;
    req->request = 8;
    req->ping = 0;
    strcpy(req->path, ".");
    snprintf(req->dest, MAXPATHLEN, "BACKUP_SS%d", unique_id);
    strncpy(req->server_ip, dest_server_ip, sizeof(req->server_ip) - 1);
    req->port = dest_server_port;
    req->kind = 2;
    req->stosport = dest_server_port;
    
    int x = send_pack_to_sserver(source_socket, req);
    free(req);
    return x;
}

void backup_server(){

    storagelist* head = sshead;
    int server_count = 1;
    while (head->next != NULL) {
        head = head->next;
        server_count++;
    }

    if(server_count == 1) return;

    if (server_count == 2){
        sshead->backup1 = head;
        head->backup1 = sshead;
        printf("Server with ID %d is being backed up on server with ID %d\n", sshead->unique_id, head->unique_id);
        printf("Server with ID %d is being backed up on server with ID %d\n", head->unique_id, sshead->unique_id);

        create_backup_folder(sshead->unique_id, head->server_ip, head->server_port,head->socket,head->stosport);
        create_backup_folder(head->unique_id, sshead->server_ip, sshead->server_port,sshead->socket,sshead->stosport);
        copy_all_files(sshead->socket, head->server_ip, head->backupport,sshead->unique_id);
        copy_all_files(head->socket, sshead->server_ip, sshead->backupport,head->unique_id);
    }
    else if (server_count == 3) {
        sshead->backup2 = head;
        sshead->next->backup2 = head;
        head->backup1 = sshead;
        head->backup2 = sshead->next;
        printf("Server with ID %d is being backed up on server with ID %d\n", sshead->unique_id, head->unique_id);
        printf("Server with ID %d is being backed up on server with ID %d\n", sshead->next->unique_id, head->unique_id);
        printf("Server with ID %d is being backed up on server with ID %d\n", head->unique_id, sshead->unique_id);
        printf("Server with ID %d is being backed up on server with ID %d\n", head->unique_id, sshead->next->unique_id);

        create_backup_folder(head->unique_id, sshead->server_ip, sshead->server_port,sshead->socket,sshead->stosport);
        create_backup_folder(head->unique_id, sshead->next->server_ip, sshead->next->server_port,sshead->next->socket,sshead->next->stosport); 
        create_backup_folder(sshead->unique_id, head->server_ip, head->server_port,head->socket,head->stosport);
        create_backup_folder(sshead->next->unique_id, head->server_ip, head->server_port,head->socket,head->stosport);

        copy_all_files(sshead->socket, head->server_ip, head->backupport,sshead->unique_id);
        copy_all_files(sshead->next->socket, head->server_ip, head->backupport,sshead->next->unique_id);
        copy_all_files(head->socket, sshead->server_ip, sshead->backupport,head->unique_id);
        copy_all_files(head->socket, sshead->next->server_ip, sshead->next->backupport,head->unique_id);
    }
    else {
        storagelist* prev1 = sshead;
        storagelist* prev2 = sshead->next;
        while (prev2->next != head) {
            prev1 = prev1->next;
            prev2 = prev2->next;
        }
        head->backup1 = prev1;
        head->backup2 = prev2;
        printf("Server with ID %d is being backed up on server with ID %d\n", head->unique_id, prev1->unique_id);
        printf("Server with ID %d is being backed up on server with ID %d\n", head->unique_id, prev2->unique_id);

        create_backup_folder(head->unique_id, prev1->server_ip, prev1->server_port,prev1->socket,prev1->stosport);
        create_backup_folder(head->unique_id, prev2->server_ip, prev2->server_port,prev2->socket,prev2->stosport);

        copy_all_files(head->socket, prev1->server_ip, prev1->backupport,head->unique_id);
        copy_all_files(head->socket, prev2->server_ip, prev2->backupport,head->unique_id);
    }
}

void addtosslist(StorageServer_init *a,int socket){
    getsswritelock();
    
    if (sshead == NULL) {
        sshead = createnode();
        sshead->unique_id = a->unique_id;
        sshead->server_port = a->sserver_port;
        strcpy(sshead->server_ip, a->sserver_ip);
        sshead->num_files = a->num_files;
        sshead->socket = socket;
        sshead->stosport = a->stosport;
        sshead->backup1 = NULL;
        sshead->backup2 = NULL;
        sshead->backupport = a->backupport;
        releasesswritelock();
        return;
    }

    sserverlist head=sshead;
    while(head->next!=NULL){
        head=head->next;
    }

    head->next=createnode();
    head=head->next;
    head->unique_id=a->unique_id;
    head->server_port=a->sserver_port;
    head->num_files=a->num_files;
    head->stosport=a->stosport;
    head->socket=socket;
    strcpy(head->server_ip,a->sserver_ip);
    head->backupport = a->backupport;
    head->backup1 = NULL;
    head->backup2 = NULL;
    
    releasesswritelock();
    return;
}
void removefromsslist(int id){
    getsswritelock();
    sserverlist head=sshead;
    sserverlist prev=NULL;
    while(head!=NULL){
        if(head->unique_id==id){
            prev->next=head->next;
            free(head);
            releasesswritelock();
            return;
        }
        prev=head;
        head=head->next;
    }
    releasesswritelock();
}
sserverlist findsserver(int id){
    getssreadlock();
    sserverlist head=sshead;
    while(head!=NULL){
        if(head->unique_id==id){
            releasessreadlock();
            return head;
        }
        head=head->next;
    }
    releasessreadlock();
    return NULL;
}
void *sserverinit(void *arg){
    struct sockaddr_in ss_addr, server_addr;
    int server_fd;
    int ss_soc;
    socklen_t addrlen = sizeof(ss_addr);
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror(RED("Socket creation failed"));
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(NS_PORT_SS);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        perror(YELLOW("set sockopt failed"));
    }
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror(RED("Bind failed"));
    }
    if (listen(server_fd, MAXSSERVER) < 0) {
        perror(RED("Listen failed"));
    }
    ss_soc = -1;
    while (1) {
        ss_soc = accept(server_fd, (struct sockaddr *)&ss_addr, &addrlen);
        if (ss_soc == -1) {
            perror(RED("Error while accepting connection"));
        }
        StorageServer_init *b = (StorageServer_init *)malloc(sizeof(StorageServer_init));
        b->num_files = MAX_FILES;
        int temp = recv(ss_soc, b, sizeof(StorageServer_init), 0);
        sserverlist temp2=findsserver(b->unique_id);
        if(temp2==NULL){
            printf(RED("Server with %s %d %d"), b->sserver_ip, b->sserver_port, b->unique_id);

            int temp_files = b->num_files;
            b->num_files = 0;

            addtosslist(b, ss_soc);
            
            for (int i = 0; i < temp_files; i++) {
                char *file = (char *)malloc(MAXFILENAME);
                temp = recv(ss_soc, file, MAXFILENAME, 0);
                build_trie(root, file, b->unique_id);

                printf(BLUE("%s\n"), file);
            }
            printf(GREEN("Server with id %d is connected\n"),b->unique_id);
            logs(1, b->unique_id, b->sserver_port, CONNECT, "Server connected", SUCCESS);

            backup_server();
        }
        else{
            getsswritelock();
            temp2->socket=ss_soc;
            releasesswritelock();
            int temp_files = b->num_files;
            for (int i = 0; i < temp_files; i++) {
                char *file = (char *)malloc(MAXFILENAME);
                temp = recv(ss_soc, file, MAXFILENAME, 0);
                printf(BLUE("%s\n"), file);
            }
            printf(GREEN("Server with id %d is connected back\n"),b->unique_id);
            logs(1, b->unique_id, b->sserver_port, CONNECT, "Server reconnected", SUCCESS);
            // update the server with backup server's data.
        }
    
        printf(BLUE("id: %d \nsserverip: %s \nsserverport: %d\n"), b->unique_id, b->sserver_ip, b->sserver_port);
        free(b);
    }
    close(server_fd);
    return NULL;
}

int pingserver(int socket){
    printf("pinging\n");
    fflush(stdout);
    clientrequest *a=(clientrequest *)malloc(sizeof(clientrequest));
    a->ping=1;
    send(socket,a,sizeof(clientrequest),0);
    logs(1, 0, socket, 0, "Ping sent", SUCCESS);
    int x=recv(socket,a,sizeof(clientrequest),0);
    if(x>0&&a->ping==1) {
        logs(1, 0, a->port, a->request, "Ping received", SUCCESS);
        return 1;
    }
    logs(1, 0, a->port, a->request, "Ping failed", STORAGE_SERVER_NOT_FOUND);
    return 0;
}

int send_pack_to_sserver(int socket, clientrequest *a){
    if(!pingserver(socket)){
        return 0;
    }
    printf("ping worked\n");
    send(socket,a,sizeof(clientrequest),0);
    logs(1, 0, a->port, a->request, "Request sent to storage server", SUCCESS);
    int x=recv(socket,a,sizeof(clientrequest),0);
    if(x>0&&a->ping==1){
        printf("%d\n",a->ping);
        logs(1, 0, a->port, a->request, "Response received from storage server", SUCCESS);
        return 1;
    }
    logs(1, 0, a->port, a->request, "Failed to receive response from storage server", STORAGE_SERVER_NOT_FOUND);
    return 0;
}