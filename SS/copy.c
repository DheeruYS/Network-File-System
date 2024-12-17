#include "headers.h"
void send_file(char *request, int client_socket) {
    char *file_name = strrchr(request, '/');
    if (file_name == NULL) {
        file_name = request;
    } else {
        file_name++;
    }
    
    // First send file size
    struct stat file_stat;
    if (stat(request, &file_stat) < 0) {
        perror("Error getting file size");
        return;
    }
    long file_size = file_stat.st_size;
    send(client_socket, &file_size, sizeof(long), 0);
    
    // Then send filename
    send(client_socket, file_name, MAXFILENAME, 0);
    printf("Sending file: %s (%ld bytes)\n", file_name, file_size);
    
    FILE *file = fopen(request, "rb");  // Open in binary mode
    if (file == NULL) {
        perror("File not found");
        return;
    }
    
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    long total_sent = 0;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        ssize_t bytes_sent = send(client_socket, buffer, bytes_read, 0);
        if (bytes_sent < 0) {
            perror("Error sending file data");
            break;
        }
        total_sent += bytes_sent;
    }
    
    fclose(file);
    printf("Finished sending %s (%ld bytes sent)\n", file_name, total_sent);
}

void receive_file(int client_socket, const char *destination) {
    char final_path[BUFFER_SIZE];
    struct stat path_stat;
    char filename[MAXFILENAME];
    
    // First receive file size
    long file_size;
    if (recv(client_socket, &file_size, sizeof(long), 0) <= 0) {
        perror("Error receiving file size");
        return;
    }
    
    // Then receive filename
    int bytes_received = recv(client_socket, filename, MAXFILENAME, 0);
    if (bytes_received <= 0) {
        perror("Error receiving filename");
        return;
    }
    filename[bytes_received] = '\0';

    // Construct final path
    if (stat(destination, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        snprintf(final_path, sizeof(final_path), "%s/%s", destination, filename);
    } else {
        snprintf(final_path, sizeof(final_path), "%s", destination);
    }

    FILE *file = fopen(final_path, "wb");  // Open in binary mode
    if (file == NULL) {
        perror("Error creating file");
        return;
    }

    char buffer[BUFFER_SIZE];
    long total_received = 0;
    
    while (total_received < file_size) {
        bytes_received = recv(client_socket, buffer, 
                            MIN(BUFFER_SIZE, file_size - total_received), 0);
        if (bytes_received <= 0) {
            perror("Error receiving file data");
            break;
        }
        
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;
    }

    fclose(file);
    printf("File received and saved as: %s (%ld bytes)\n", final_path, total_received);
}
void remove_filename(char *full_path) {
    // Find the last occurrence of '/' in the path
    char *last_slash = strrchr(full_path, '/');

    // If a '/' is found, terminate the string before it
    if (last_slash != NULL) {
        *last_slash = '\0';
    }
}
void receive_directory(int client_socket, const char *dest_dir) {
    char buffer[MAXFILENAME] = {0};

    while (1) {
        memset(buffer,0,MAXFILENAME);
        int bytes_received = recv(client_socket, buffer, MAXFILENAME, 0);
        if (bytes_received <= 0 || strcmp(buffer, "EOD") == 0) {
            break;
        }
        // printf("%s\n",buffer);

        // Handling directory creation
        if (strncmp(buffer, "DIR:", 4) == 0) {
            char dir_name[MAXFILENAME];
            sscanf(buffer + 4, "%s", dir_name);
            char full_path[MAXFILENAME];
            snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir, dir_name);
            mkdir(full_path, 0777);
            printf("Directory created: %s\n", full_path);
        } 
        // Handling file reception
        else if (strncmp(buffer, "FILE:", 5) == 0) {
            char file_name[MAXFILENAME];
            sscanf(buffer + 5, "%s", file_name);
            char full_path[MAXFILENAME];
            snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir, file_name);
            // printf("this one:  %s\n",full_path);
            remove_filename(full_path);
            receive_file(client_socket, full_path);
        }

        memset(buffer, 0, sizeof(buffer)); // Clear buffer for next message
    }
}
void send_directory(const char *dir_name, int client_socket, const char *parent_path) {

    if(strncmp(dir_name,"BACKUP",6)==0) return;

    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    // Always send the current directory, including root
    char dir_message[MAXFILENAME];
    if (strcmp(parent_path, "") != 0) {
        snprintf(dir_message, sizeof(dir_message), "DIR:%s", parent_path);
    } else {
        // For root directory, use its name directly
        const char *last_slash = strrchr(dir_name, '/');
        const char *dir_only = last_slash ? last_slash + 1 : dir_name;
        snprintf(dir_message, sizeof(dir_message), "DIR:%s", dir_only);
    }
    send(client_socket, dir_message, MAXFILENAME, 0);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if(strncmp(entry->d_name,"BACKUP",6)==0) continue;

        // Construct the full path for the current entry
        char full_path[MAXFILENAME];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, entry->d_name);

        // Construct the relative path for messages
        char relative_path[MAXFILENAME];
        if (strcmp(parent_path, "") != 0) {
            snprintf(relative_path, sizeof(relative_path), "%s/%s", parent_path, entry->d_name);
        } else {
            const char *last_slash = strrchr(dir_name, '/');
            const char *dir_only = last_slash ? last_slash + 1 : dir_name;
            snprintf(relative_path, sizeof(relative_path), "%s/%s", dir_only, entry->d_name);
        }

        struct stat file_stat;
        if (stat(full_path, &file_stat) < 0) {
            perror("Error getting file status");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            // Send DIR message for the current directory
            char message[MAXFILENAME];
            snprintf(message, sizeof(message), "DIR:%s", relative_path);
            // send(client_socket, message, MAXFILENAME, 0);
            
            // Recursively process subdirectory
            send_directory(full_path, client_socket, relative_path);
        } 
        // else if (S_ISREG(file_stat.st_mode)) {
        //     // Send FILE message for files
        //     char message[MAXFILENAME];
        //     snprintf(message, sizeof(message), "FILE:%s", relative_path);
        //     // printf("%s\t%s\n",message,full_path);
        //     send(client_socket, message, MAXFILENAME, 0);
        //     send_file(full_path,client_socket);
        // }
    }

    closedir(dir);
}
void send_directoryfiles(const char *dir_name, int client_socket, const char *parent_path) {

    if(strncmp(dir_name,"BACKUP",6)==0) return;

    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    // Always send the current directory, including root
    char dir_message[MAXFILENAME];
    if (strcmp(parent_path, "") != 0) {
        snprintf(dir_message, sizeof(dir_message), "DIR:%s", parent_path);
    } else {
        // For root directory, use its name directly
        const char *last_slash = strrchr(dir_name, '/');
        const char *dir_only = last_slash ? last_slash + 1 : dir_name;
        snprintf(dir_message, sizeof(dir_message), "DIR:%s", dir_only);
    }
    // send(client_socket, dir_message, MAXFILENAME, 0);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if(strncmp(entry->d_name,"BACKUP",6)==0) continue;

        // Construct the full path for the current entry
        char full_path[MAXFILENAME];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, entry->d_name);

        // Construct the relative path for messages
        char relative_path[MAXFILENAME];
        if (strcmp(parent_path, "") != 0) {
            snprintf(relative_path, sizeof(relative_path), "%s/%s", parent_path, entry->d_name);
        } else {
            const char *last_slash = strrchr(dir_name, '/');
            const char *dir_only = last_slash ? last_slash + 1 : dir_name;
            snprintf(relative_path, sizeof(relative_path), "%s/%s", dir_only, entry->d_name);
        }

        struct stat file_stat;
        if (stat(full_path, &file_stat) < 0) {
            perror("Error getting file status");
            continue;
        }

        if (S_ISDIR(file_stat.st_mode)) {
            // Send DIR message for the current directory
            char message[MAXFILENAME];
            snprintf(message, sizeof(message), "DIR:%s", relative_path);
            // send(client_socket, message, MAXFILENAME, 0);
            
            // Recursively process subdirectory
            send_directoryfiles(full_path, client_socket, relative_path);
        } else if (S_ISREG(file_stat.st_mode)) {
            // Send FILE message for files
            char message[MAXFILENAME];
            snprintf(message, sizeof(message), "FILE:%s", relative_path);
            // printf("%s\t%s\n",message,full_path);
            send(client_socket, message, MAXFILENAME, 0);
            send_file(full_path,client_socket);
        }
    }

    closedir(dir);
}