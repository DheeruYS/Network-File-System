#include "headers.h"

// Function to get local IP address
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
                printf(RED("getnameinfo() failed: %s in SS/func.c\n"), gai_strerror(s));
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

// Function to register with naming server
int register_with_naming_server(StorageServer_init *server, StorageServer_send *send_data, char** files) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror(RED("Socket creation failed"));
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server->naming_server_port);
    
    if (inet_pton(AF_INET, server->naming_server_ip, &server_addr.sin_addr) <= 0) {
        perror(RED("Invalid address"));
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror(RED("Connection failed"));
        return -1;
    }

    // Prepare registration message
    if (send(sock, send_data, sizeof(StorageServer_send), 0) < 0) {
        perror(RED("Registration send failed"));
        return -1;
    }
    for(int i=0; i<send_data->num_files; i++) {
        if (send(sock, files[i], MAXFILENAME, 0) < 0) {
            perror(RED("Filename send failed"));
            return -1;
        }
    }
    return sock;
}

void calculateRelativeDirectory(char *cwd, char *home, char *relative_directory) {
    size_t home_len = strlen(home);
    size_t cwd_len = strlen(cwd);

    if (strncmp(cwd, home, home_len) == 0) {
        strcpy(relative_directory, cwd + home_len+1);
    } else {
        strcpy(relative_directory, cwd);
    }
}

char **get_files(char *path, int *num_files) {
    DIR *dir;
    struct dirent *entry;
    char **files = (char **)malloc(MAX_FILES * sizeof(char *));
    for (int i = 0; i < MAX_FILES; i++) {
        files[i] = NULL;
    }
    int count = 0;

    if ((dir = opendir(path)) == NULL) {
        perror("opendir");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            files[count] = (char *)malloc(MAXPATHLEN * sizeof(char));
            snprintf(files[count], MAXPATHLEN, "%s/%s", path, entry->d_name);
            // Convert to relative path
            char relative_path[MAXPATHLEN];
            calculateRelativeDirectory(files[count], cwd, relative_path);
            strcpy(files[count], relative_path);  // Copy the relative path back
            count++;
        } else if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Construct the new path for the subdirectory
                char subdir[MAXPATHLEN];
                snprintf(subdir, sizeof(subdir), "%s/%s", path, entry->d_name);
                
                // Add subdirectory path itself as a relative path
                files[count] = (char *)malloc(MAXPATHLEN * sizeof(char));
                calculateRelativeDirectory(subdir, cwd, files[count]);
                count++;

                // Recursive call to process subdirectory
                int subdir_num_files;
                char **subdir_files = get_files(subdir, &subdir_num_files);

                // Add subdirectory files to the list
                for (int i = 0; i < subdir_num_files; i++) {
                    if (count < MAX_FILES) {
                        files[count] = (char *)malloc(MAXPATHLEN * sizeof(char));
                        strcpy(files[count], subdir_files[i]);
                        count++;
                    }
                    free(subdir_files[i]);
                }
                free(subdir_files);
            }
        }
        if (count >= MAX_FILES) {
            printf(RED("Maximum number of files reached\n"));
            break;
        }
    }
    closedir(dir);

    *num_files = count;
    return files;
}