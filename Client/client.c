# include "client.h"
#define STREAM_FIFO "/tmp/stream_fifo"
pid_t mpv_pid = -1;
#define BACK_CHANNEL_PORT 9000 // this is the unique port on which the client will be listening for any async acks

// known function
void man()
{
    printf("Commands available:\n");
    printf(GREEN ("  READ <path> - Read a file\n" ));
    printf(GREEN ("  WRITE <path> - Write to a file\n"));
    printf(GREEN ("  WRITEASYNC <path> - Write to a file asynchronously\n"));
    printf(GREEN ("  DELETE <directory_path> <filename>- Delete a file\n"));
    printf(GREEN ("  DELETE <directory> - Delete a directory\n"));
    printf(GREEN ("  GETINFO <path> - Get information about a file or directory\n"));
    printf(GREEN ("  STREAM <path> - Stream a music file\n"));
    printf(GREEN ("  CREATE <path> <name> - Create a file\n"));
    printf(GREEN ("  CREATE <directory_path> <name> - Create a folder\n"));
    printf(GREEN ("  COPY <source> <dest> - Copy a file\n"));
    printf(GREEN ("  APPEND <path> - Append to a file\n"));
    printf(GREEN ("  LIST <path> - List all files in a directory (Press ~ for root)\n"));
    printf(GREEN ("  HELP - Show this help message\n"));
    printf(GREEN ("  clear - Clear the screen\n"));
    printf(GREEN ("  exit - Exit the program\n"));
    return;
}

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


void *handle_back_channel_communication(void *arg) {
    int back_channel_fd;
    struct sockaddr_in back_channel_addr, client_addr;
    socklen_t addrlen = sizeof(client_addr);
    char buffer[MAXDATASIZE];

    // Create socket for back channel communication
    back_channel_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (back_channel_fd < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    // Set up the address structure for the back channel
    back_channel_addr.sin_family = AF_INET;
    back_channel_addr.sin_port = htons(BACK_CHANNEL_PORT);
    back_channel_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(back_channel_fd, (struct sockaddr *)&back_channel_addr, sizeof(back_channel_addr)) < 0) {
        perror("Bind failed");
        close(back_channel_fd);
        return NULL;
    }

    // Listen for incoming connections
    if (listen(back_channel_fd, 5) < 0) {
        perror("Listen failed");
        close(back_channel_fd);
        return NULL;
    } 
  

    // printf("Listening for back channel communication on port %d...\n", BACK_CHANNEL_PORT);

    while (1) {
        int client_socket = accept(back_channel_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Receive data from the server
        async_ack ack;
        int bytes_received = recv(client_socket, &ack, sizeof(ack), 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received data
            printf("Received data: %s\n", ack.data);
            // Process the received data as needed
        } else {
            perror("Receive failed");
        }

        close(client_socket); // Close the client socket after processing
    }

    close(back_channel_fd);
    return NULL;
}

void handle_kind_3_request(int client_socket_fd) {
    char buffer[20000] = {'\0'};
    buffer[0] = '\0';
    int bytes_received;

    setbuf(stdout, NULL); // Ensure unbuffered output for immediate printing

    // Directly print data as it's received
    while ((bytes_received = recv(client_socket_fd, buffer, sizeof(buffer), 0)) > 0) {
            buffer[bytes_received] = '\0';  
            printf("%s", buffer);
        fflush(stdout); 

        if (strstr(buffer , "No More files / directories found") != NULL) { 
            break;
        }

        memset(buffer, 0, sizeof(buffer)); // Clear the buffer after printing
    }

    // Check for errors in receiving data
    if (bytes_received < 0) {
        perror("recv");
    }
}


void print_request(const incomingreqclient *request) {
    printf("Client Request:\n");
    printf("  Type: %d\n", request->type);
    printf("  Path: %s\n", request->path);
}

int play_stream_via_mpv() {
    mkfifo(STREAM_FIFO, 0666); // Ensure the FIFO exists
    mpv_pid = fork(); // Create a child process for mpv
    if (mpv_pid == 0) {
        // Child process: Launch mpv with GUI
        execlp("mpv", "mpv", 
               "--no-terminal",               // Suppress terminal logs
               "--player-operation-mode=pseudo-gui", // Enable GUI controls
               "--input-ipc-server=/tmp/mpvsocket", // IPC for additional control
               STREAM_FIFO,                   // Input stream
               (char *)NULL);
        perror("Failed to start mpv");
        exit(1);
    }
    return mpv_pid;
}
int errorCodeHandler(int errorCode)
{
    switch (errorCode) {
        case 0:
            return 1; // SUCCESS
        case 404:
            printf(RED("No such file exists\n"));
            return -1;
        case 400:
            printf(RED("File already exists\n"));
            return -1;
        case 505:
            printf(RED("Storage server not found\n"));
            return -1;
        case 999:
            printf(RED("Access denied\n"));
            return -1;
        case 1001:
            printf(RED("File not created\n"));
            return -1;
        case 1000:
            printf(RED("File not deleted\n"));
            return -1;
        case 100:
            printf(RED("Invalid path\n"));
            return -1;
        case 200:
            printf(RED("Client server disconnected\n"));
            return -1;
        case 600:
            printf(RED("Timeout\n"));
            return -1;
        default:
            printf(RED("Unknown error\n"));
            return -1;
    }
}

int send_request_to_storage(const char *ip, int port, const incomingreqclient *request) {
    struct addrinfo hints, *res;
    int storage_socket_fd;
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status = getaddrinfo(ip, port_str, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }

    storage_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(storage_socket_fd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect to storage failed");
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    // print_request(request);
    if (send(storage_socket_fd, request, sizeof(*request), 0) == -1) {
        perror("send to storage");
        close(storage_socket_fd);
        return -1;
    }

    char buffer[MAXDATASIZE];
    int bytes_received;

    // Handle response based on request type
    if (request->type == CMD_STREAM) {
        printf("Enjoy the vibes! \n"); 
        play_stream_via_mpv();

        // Open FIFO and stream data to mpv
        int fifo_fd = open(STREAM_FIFO, O_WRONLY);
        if (fifo_fd == -1) {
            perror("Failed to open FIFO for writing");
            close(storage_socket_fd);
            return -1;
        }

        while ((bytes_received = recv(storage_socket_fd, buffer, sizeof(buffer), 0)) > 0) {
            if (write(fifo_fd, buffer, bytes_received) == -1) {
                perror("Error writing to FIFO");
                break;
            }
        }
        
        close(fifo_fd);
        unlink(STREAM_FIFO); 
    }
    else if (request->type == CMD_WRITE || request->type == CMD_WRITE_ASYNC) {      
        // char buffer[4096];
        // printf("Enter the text to write to the file (Press Ctrl-D in a new line to stop): ");
        // fflush(stdout);

        // while (1) {
        //     memset(buffer, 0, sizeof(buffer));
        //     ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            
        //     if (bytes_read <= 0) {  // EOF or error
        //         const char *eof_message = "END\n";
        //         send(storage_socket_fd, eof_message, strlen(eof_message), 0);
        //         break;
        //     }

        //     // Send the data to the server
        //     ssize_t bytes_sent = send(storage_socket_fd, buffer, bytes_read, 0);
        //     if (bytes_sent == -1) {
        //         perror("Error sending data to socket");
        //         break;
        //     }
        // }

        // printf("Finished writing to file.\n");
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            printf("> "); // Prompt for input
            fflush(stdout);

            // Read a line of input from stdin
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                perror("Error reading input");
                break;
            }

            // Check if the user typed "END\n"
            if (strncmp(buffer, "END\n", 4) == 0) {
                const char *eof_message = "END\n";
                send(storage_socket_fd, eof_message, strlen(eof_message), 0);
                break;
            }

            // Send the line of input to the server
            ssize_t bytes_sent = send(storage_socket_fd, buffer, strlen(buffer), 0);
            if (bytes_sent == -1) {
                perror("Error sending data to socket");
                break;
            }
        }
        // printf("after this\n");
        // Wait for and receive the complete acknowledgment
        ss_to_client_ack ack;
        memset(&ack, 0, sizeof(ack));
        size_t ack_size = sizeof(ack);
        size_t bytes_received = 0;

        while (bytes_received < ack_size) {
            ssize_t result = recv(storage_socket_fd, ((char *)&ack) + bytes_received, ack_size - bytes_received, 0);
            if (result < 0) {
                perror("recv error");
                break;
            } else if (result == 0) {
                printf("Connection closed unexpectedly\n");
                break;
            }
            bytes_received += result;
        }

        if (bytes_received == ack_size) {
            if (ack.errorCode == 0) {
                printf(GREEN("Success: %s \n"), ack.ack_message);
            } else {
                printf(RED("[%d] Error: %s\n"), ack.errorCode, ack.ack_message);
            }
        } else {
            printf("Error receiving acknowledgment\n");
        }
    }
     else if (request->type == CMD_APPEND) {      
    printf("Enter the text to append to  the file (Press Ctrl-D in a new line to stop): ");
    fflush(stdout);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        
        if (bytes_read <= 0) {  // EOF or error
            const char *eof_message = "END\n";
            send(storage_socket_fd, eof_message, strlen(eof_message), 0);
            break;
        }

        // Send the data to the server
        ssize_t bytes_sent = send(storage_socket_fd, buffer, bytes_read, 0);
        if (bytes_sent == -1) {
            perror("Error sending data to socket");
            break;
        }
    }

    printf("Finished writing to file.\n");

        printf("after this\n");
        // Wait for and receive the complete acknowledgment
        ss_to_client_ack ack;
        memset(&ack, 0, sizeof(ack));
        size_t ack_size = sizeof(ack);
        size_t bytes_received = 0;

        while (bytes_received < ack_size) {
            ssize_t result = recv(storage_socket_fd, ((char *)&ack) + bytes_received, ack_size - bytes_received, 0);
            if (result < 0) {
                perror("recv error");
                break;
            } else if (result == 0) {
                printf("Connection closed unexpectedly\n");
                break;
            }
            bytes_received += result;
        }

        if (bytes_received == ack_size) {
            if (ack.errorCode == 0) {
                printf(GREEN("Success: %s \n"), ack.ack_message);
            } else {
                printf(RED("[%d] Error: %s\n"), ack.errorCode, ack.ack_message);
            }
        } else {
            printf("Error receiving acknowledgment\n");
        }
    }
    else if(request->type == CMD_READ){ 
        setbuf(stdout, NULL);
        // Directly print data if not a stream request
        while ((bytes_received = recv(storage_socket_fd, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, bytes_received, stdout);  // Print to standard output
        }
        // fflush(stdout);
    }
    else if(request->type == CMD_GET_INFO) {
        
        setbuf(stdout, NULL);
        char received_data[4096] = {0};  // Larger buffer to store complete response
        size_t total_received = 0;
        
        // Receive all data first
        while ((bytes_received = recv(storage_socket_fd, buffer, sizeof(buffer), 0)) > 0) {
            if (total_received + bytes_received < sizeof(received_data)) {
                memcpy(received_data + total_received, buffer, bytes_received);
                total_received += bytes_received;
            }
        }
        received_data[total_received] = '\0';

        // Print raw output for debugging
        // printf("Raw server response:\n%s\n", received_data);

        // Process each line
        char *saveptr;
        char *line = strtok_r(received_data, "\n", &saveptr);
        
        while (line != NULL) {
            // Skip empty lines
            if (strlen(line) == 0) {
                line = strtok_r(NULL, "\n", &saveptr);
                continue;
            }

            // Parse only lines that start with expected ls -l format
            if (line[0] == '-' || line[0] == 'd' || line[0] == 'l') {
                char permissions[11] = {0};
                char user[50] = {0};
                char group[50] = {0};
                char month[4] = {0};
                char time[6] = {0};
                char name[256] = {0};
                int links = 0;
                long long size = 0;
                int day = 0;

                // Parse the ls -l output with more precise format
                int matched = sscanf(line, "%10s %d %49s %49s %lld %3s %d %5s %255[^\n]",
                                permissions, &links, user, group, &size,
                                month, &day, time, name);

                if (matched == 9) {  // Only print if we successfully parsed all fields
                    printf("\033[1;34m\n");
                    
                    printf("\nFile Information:\n");
                    // printf("Name: %s\n", name);
                    printf("Size: %lld bytes\n", size);
                    printf("Permissions: %s\n", permissions);
                    printf("Owner: %s\n", user);
                    printf("Group: %s\n", group);
                    printf("Last Modified: %s %d %s\n", month, day, time);
                    
                    printf("Number of Links: %d\n", links);
                    printf("\033[0m\n");
                }
            }
            
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }
    if (bytes_received == -1) {
        perror("recv error from storage");
    } else if (bytes_received == 0) {
        close(storage_socket_fd);
        printf("Connection closed by the server.\n");
    }
    return 0;
}

int isUserRequestValid(char * str , incomingreqclient *request)
{
    int request_type = 0;
    char arg1[65535], arg2[65535];
    char command[16];
    int args_count = sscanf(str, "%15s %65534s %65534s", command, arg1, arg2);
      
    if (strcmp(str, "exit") == 0) {
        printf("Goodbye! See you again soon!\n");
        return EXIT_STATUS;
    }
    else if (strcmp(str, "clear") == 0) {
        system("clear");
        return CLEAR_STATUS;
    }


    // Normal command checking
    if (args_count < 2) 
    {  
        printf("Enter valid arguments\n");   
        return -1; 
    }
    if (args_count < 3) 
    { 
        arg2[0] = '\0';
    }
  
     if(strncmp(str, "READ", 4) == 0) // reading a file : SS 
    {
        request_type = CMD_READ; // path
        request->kind = KIND_1; 
    }
    else if(strncmp(str, "WRITEASYNC", 10) == 0) // writing a file / folder : SS asynchrounously
    {
        request_type = CMD_WRITE_ASYNC; // path
        request->kind = KIND_1; 
    }
    else if(strncmp(str, "WRITE" , 5) == 0) // writing a file / folder : SS
    {
        request_type = CMD_WRITE; // path
         request->kind = KIND_1; 
    }
    else if(strncmp(str, "DELETE" , 6) == 0) // deleting a file / folder : NS
    {
        request_type = CMD_DELETE; // path 
        request->kind = KIND_2; 
    }
    else if(strncmp(str, "GETINFO", 7 ) == 0) // getting additionaly information about the file : SS
    {
        request_type = CMD_GET_INFO; // path 
         request->kind = KIND_1; 
    }
    else if(strncmp(str, "STREAM", 6) == 0) // stream of music files : SS
    {
        request_type = CMD_STREAM; // path 
        request->kind = KIND_1; 
    }
    else if(strncmp(str, "CREATE", 6) == 0) // creating a file or folder : NS
    {
        request_type = CMD_CREATE; // path and name 
        request->kind = KIND_2; 
    }
    else if(strncmp(str, "COPY" , 4) == 0) // copying of files : NS
    {
        request_type = CMD_COPY; // source , dest  : path , destination 
        request->kind = KIND_2; 
    }
    else if(strncmp(str, "LIST" ,4) == 0) // listing all the files : NS 
    {
        request_type = CMD_LIST; // path 
        request->kind = KIND_3; 
    }
    else if(strncmp(str, "APPEND" , 5) == 0) // writing a file / folder : SS
    {
        request_type = CMD_APPEND; // path
        request->kind = KIND_1; 
    }
    else if(strncmp(str, "HELP" , 4) == 0) // man/help commmand
    {
        man();
        return -1;
    }
    
    if(request_type == 0)
    {
        return -1;
    }
    request->type = request_type;
    if (request_type == CMD_DELETE && (args_count ==3)) { // Path and file : DELETE DIR FILEMAN
        strncpy(request->path, arg1, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
        strncpy(request->file, arg2, sizeof(request->file) - 1);
        request->file[sizeof(request->file) - 1] = '\0';
        return 0;
    } 
    else if (request_type == CMD_DELETE && (args_count ==2)) { // DELETE directory 
        strncpy(request->path, arg1, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
        return 0;
    }
   else if ((request_type <= 5 || request_type == CMD_LIST || request_type == CMD_APPEND || request_type == CMD_WRITE_ASYNC) && args_count == 2) {
        strncpy(request->path, arg1, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
        return 0;
    }
    else if (request_type == CMD_CREATE && (args_count ==3)) { // we are specifying a CREATE path Filename
        strncpy(request->path, arg1, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
        strncpy(request->file, arg2, sizeof(request->file) - 1);
        request->file[sizeof(request->file) - 1] = '\0';
        return 0;
    } 
    else if (request_type == CMD_CREATE && (args_count ==2)) { // we are specifying a CREATE path (directory)
        strncpy(request->path, arg1, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
        return 0;
    }
    else if (request_type == CMD_COPY && args_count == 3) {
        strncpy(request->path, arg1, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
        strncpy(request->dest, arg2, sizeof(request->dest) - 1);
        request->dest[sizeof(request->dest) - 1] = '\0';
        return 0;
    }
    else 
    {
        printf("Enter valid arguments\n");
        return -1;
    }
}

int main (int argc , char * argv[])
{
    man();
    // parsing the IP of naming server and establishing a connection 
    // Using TCP connection to create a socket and communicate with the naming server 
    struct addrinfo hints, *res; // structure for getaddrinfo
    memset(&hints, 0 , sizeof hints);

    int client_socket_fd;

    char * server_ip;
    char * server_port;
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }

    server_ip = argv[1];
    server_port = argv[2];
    hints.ai_family = AF_UNSPEC; // can use either IPV4 or IPV6
    hints.ai_socktype = SOCK_STREAM; // we are using TCP sockets
    int status; 
    printf("Connected successfully to port : %s\n", server_port);
    // getting information about the server_ip and the server_port and using it to create out socket
    if ((status = getaddrinfo(server_ip, server_port , &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    client_socket_fd = socket(res->ai_family , res->ai_socktype , res->ai_protocol); 

    if(connect(client_socket_fd , res->ai_addr , res->ai_addrlen) == -1)
    {
        perror("connect failed");
        exit(1);
    }

    freeaddrinfo(res); // we no longer need it 

    pthread_t back_channel_thread;
    if (pthread_create(&back_channel_thread, NULL, handle_back_channel_communication, NULL) != 0) {
        perror("Failed to create back channel thread");
        return 1;
    }
    
    int bytes_received;
    char buffer[MAXDATASIZE];
    char user_request[MAXDATASIZE]; 
    while (1)
    {
        // asking for user input and sending it to the client  
        // Defining all the input the client can say 
        printf("Enter your request : ");
        scanf(" %[^\n]", user_request);
        incomingreqclient request = {0};
        // is User request checks if the request is valid or not 
        // also puts all the values
        int ret_val = isUserRequestValid(user_request ,&request);
        if(ret_val == -1)
        {
            printf("Enter again\n");
            continue;
        }
        else if(ret_val == EXIT_STATUS)
        {
            exit(EXIT_SUCCESS);
        }
        else if(ret_val == CLEAR_STATUS)
        {
            // just clearing of screen 
            continue;
        }
            get_local_ip(request.client_ip);
            request.async_port = BACK_CHANNEL_PORT;
            print_request(&request);  
            // printf("Sending the data\n");
        if (send(client_socket_fd, &request, sizeof(request), 0) == -1) {
            perror("send");
            continue;
        }
            // printf("Sent the data\n");


        if (request.kind == KIND_1) {
            // Expecting a kind1Response
            kind1Response response;
            bytes_received = recv(client_socket_fd, &response, sizeof(response), 0);

            if (bytes_received == -1) 
            {
                perror("recv");
            } 
            else if (bytes_received == 0) {
                printf("Server closed the connection\n");
                break;
            } 
            else if(errorCodeHandler(response.errorCode) == 1)
            {
                printf("Server Response:\n");
                printf("  Storage Server IP: %s\n", response.storageServerIP);
                printf("  Storage Server Port: %d\n", response.storageServerPort);
                printf("  Error Code: %d\n", response.errorCode);
                strcpy(request.path,response.fileName);

                send_request_to_storage(response.storageServerIP , response.storageServerPort , &request);
                
                // if(response.backup1ServerPort != -1){
                //     snprintf(request.path, sizeof(request.path), "BACKUP_SS%d/./%s",response.unique_id, response.fileName);
                //     send_request_to_storage(response.backup1ServerIP , response.backup1ServerPort , &request);
                // }
                // if(response.backup2ServerPort != -1){
                //     snprintf(request.path, sizeof(request.path), "BACKUP_SS%d/./%s",response.unique_id, response.fileName);
                //     send_request_to_storage(response.backup2ServerIP , response.backup2ServerPort , &request);
                // }
            }
            send(client_socket_fd,&response,sizeof(response),0);
        } 
        else if (request.kind == KIND_2) {
            // Expecting a kind2Ack
            kind2Ack ack;
            bytes_received = recv(client_socket_fd, &ack, sizeof(ack), 0);

            if (bytes_received == -1) {
                perror(RED("recv"));
            } else if (bytes_received == 0) {
                printf(RED("Server closed the connection\n"));
                break;
            } else {
                if(ack.errorCode == SUCCESS){
                    printf(GREEN("Acknowledgment from Server:\n"));
                    printf(GREEN("  Message: %s\n"), ack.ack_message);
                }
                else{
                    printf(RED("Acknowledgment from Server:\n"));
                    printf(RED(" [%d] Message: %s\n"), ack.errorCode, ack.ack_message);
                }
            }
            // send(client_socket_fd,&ack,sizeof(ack),0);
        }
        else if(request.kind == KIND_3)
        {
            handle_kind_3_request(client_socket_fd);
        }
    }
    pthread_join(back_channel_thread, NULL);
}