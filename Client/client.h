#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <ifaddrs.h>

# define MAXDATASIZE 100
# define MAXPATHLEN 200
# define MAXFILENAME 100
typedef struct incomingreqclient{
    int type;
    char path[MAXPATHLEN];
    char dest[MAXPATHLEN];
    char file[MAXFILENAME];
    int kind; 
    char client_ip[16];
    int async_port;
}incomingreqclient;

typedef struct kind1Response{
    char storageServerIP[20]; 
    int storageServerPort;
    char backup1ServerIP[20]; 
    int backup1ServerPort;
    char backup2ServerIP[20]; 
    int backup2ServerPort;
    int errorCode;
    char fileName[MAXFILENAME];
    int unique_id;
}kind1Response;

typedef struct kind2Ack {
    char ack_message[MAXDATASIZE];
    int errorCode;
}kind2Ack;

typedef struct ss_to_client_ack {
    char ack_message[MAXDATASIZE];
    int errorCode;
}ss_to_client_ack;

typedef struct async_ack{
    char client_ip[16];
    int async_port;
    char data[100];
}async_ack;

#define CMD_READ 1 // Kind 1 request
#define CMD_WRITE 2 // Kind 1 request 
#define CMD_DELETE 3 // Kind 2 request
#define CMD_GET_INFO 4 // Kind 1 request
#define CMD_STREAM 5 // Kind 
#define CMD_CREATE 6 // Kind 
#define CMD_COPY 7 
#define CMD_LIST 8
#define CMD_APPEND 9
#define CMD_WRITE_ASYNC 10

# define EXIT_STATUS 10 
# define CLEAR_STATUS 9
# define KIND_1 1 
# define KIND_2 2 
# define KIND_3 3

// Colors
#define RESET_COLOR  "\033[0m"                  
#define RED_COLOR    "\033[1;31m"               // ERROR with storage server
#define GREEN_COLOR  "\033[0;32m"               // SUCCESS
#define BLUE_COLOR   "\033[0;34m"               // DEBUGGING with storage server
#define YELLOW_COLOR "\033[1;33m"               // ERROR with client server
#define CYAN_COLOR   "\033[0;36m"               // NORMAL DEBUGGING
#define PINK_COLOR   "\033[38;2;255;182;193m"   // DEBUGGING with client server
#define ORANGE_COLOR "\e[38;2;255;85;0m"        

#define RED(str)    RED_COLOR    str RESET_COLOR
#define GREEN(str)  GREEN_COLOR  str RESET_COLOR
#define BLUE(str)   BLUE_COLOR   str RESET_COLOR
#define YELLOW(str) YELLOW_COLOR str RESET_COLOR
#define CYAN(str)   CYAN_COLOR   str RESET_COLOR
#define PINK(str)   PINK_COLOR   str RESET_COLOR
#define ORANGE(str) ORANGE_COLOR str RESET_COLOR

#define SUCCESS 0
#define FILE_ALREADY_EXISTS 400
#define FILE_NOT_FOUND 404
#define STORAGE_SERVER_NOT_FOUND 505
#define ACCESS_DENIED 999
#define FILE_NOT_CREATED 1001
#define FILE_NOT_DELETED 1000
#define INVALID_PATH 100
#define CLIENT_SERVER_DISCONNECTED 200
#define TIMEOUT 600

#endif