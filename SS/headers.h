#ifndef __HEADERS_H__
#define __HEADERS_H__
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define IP_ADDRESS "127.0.0.1"
#define SS_PORT 8005  // Port for client connections
#define S_SSPORT 8010 // for ss-ss comms
#define BACKUP_PORT 8011
#define MAX_BUFFER 1024
#define MAXPATHLEN 200
#define MAXFILENAME 100
#define MAX_FILES 100
#define UNIQUE_ID 300
#define BUFFER_SIZE 1024
#define MAXSSERVER 100
#define MAXDATASIZE 100
#define WRITEBUFFER 4096
// Colors
#define RESET_COLOR  "\033[0m"                  
#define RED_COLOR    "\033[1;31m"               // ERROR with naming server
#define GREEN_COLOR  "\033[0;32m"               // SUCCESS
#define BLUE_COLOR   "\033[0;34m"               // DEBUGGING with naming server
#define YELLOW_COLOR "\033[1;33m"               // ERROR with storage server
#define CYAN_COLOR   "\033[0;36m"
#define PINK_COLOR   "\033[38;2;255;182;193m"   // DEBUGGING with storage server
#define ORANGE_COLOR "\e[38;2;255;85;0m"        

#define RED(str)    RED_COLOR    str RESET_COLOR
#define GREEN(str)  GREEN_COLOR  str RESET_COLOR
#define BLUE(str)   BLUE_COLOR   str RESET_COLOR
#define YELLOW(str) YELLOW_COLOR str RESET_COLOR
#define CYAN(str)   CYAN_COLOR   str RESET_COLOR
#define PINK(str)   PINK_COLOR   str RESET_COLOR
#define ORANGE(str) ORANGE_COLOR str RESET_COLOR

extern char* cwd ;
// Structs
typedef struct {
    int unique_id;
    char naming_server_ip[16];
    int naming_server_port;
    char local_ip[16];
    int local_port;
} StorageServer_init;

typedef struct {
    int unique_id;
    char ss_ip[20];
    int ss_port;
    int num_files;
    int stosport;
    int backupport;
} StorageServer_send;

typedef struct clientrequest{
    int socket;
    int request;
    char path[MAXPATHLEN];
    char dest[MAXPATHLEN];
    char file[MAXFILENAME];
}clientrequest;

typedef struct incomingreqclient{
    int type;
    char path[MAXPATHLEN];
    char dest[MAXPATHLEN];
    char file[MAXFILENAME];
    int kind;
    char client_ip[16];
    int async_port;
}incomingreqclient;

typedef struct nsrequest{
    int socket;
    int request;
    int ping;
    char path[MAXPATHLEN];
    char dest[MAXPATHLEN];
    char file[MAXFILENAME];
    int kind;
    char server_ip[20];
    int port;
    int stosport;
}nsrequest;

typedef struct ss_to_client_ack {
    char ack_message[MAXDATASIZE];
    int errorCode;
}ss_to_client_ack;

typedef struct stoscopy{
    char buffer[MAXDATASIZE];
    int isdir;
    int isfile;
    int end;
    int isname;
}stoscopy;

typedef struct async_ack{
    char client_ip[16];
    int async_port;
    char data[100];
}async_ack;

//sshandler.c
void *storage_server_handler(void *arg);
void *storage_connect(void *arg);
void *backup_connect(void *arg);
void *executeserver(void *arg);
void *executebackup(void *arg);
int checkPath(char *path);
    
// func.c
int register_with_naming_server(StorageServer_init *server, StorageServer_send *send, char** files);
char* get_local_ip(char* buffer);
void calculateRelativeDirectory(char *cwd, char *home, char *relative_directory);
char **get_files(char *path, int *num_files);
void *naming_server_listener(void *arg);
void *client_server_handler(void *arg);
void *backup_server_handler(void *arg);

// read_write.c
void write_fn(clientrequest * a, int isAppend);
void read_fn(clientrequest * a);
int async_write_fn(clientrequest * a, int isAppend);

//copy.c
void send_file(char *request, int client_socket);
void send_directory(const char *dir_name, int client_socket, const char *parent_path);
void send_directoryfiles(const char *dir_name, int client_socket, const char *parent_path);
void receive_file(int client_socket, const char *destination);
void receive_directory(int client_socket, const char *dest_dir);
void send_file2(FILE *file, int client_socket);

//ss.c
void send_data_to_ns(async_ack * ack);

#endif