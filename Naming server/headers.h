#ifndef __HEADERS_H__
#define __HEADERS_H__
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>

#define MAXCLIENTS 100
#define MAXPATHLEN 200
#define MAX_FILES 100
#define MAXFILENAME 100
#define NS_PORT 8080
#define NS_PORT_SS 8000
#define MAXSSERVER 100
#define ALPHABET_SIZE 256
#define MAXDATASIZE 100
#define CACHE_SIZE 2

typedef struct clientrequest{
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

typedef struct StorageServer_init{
    int unique_id;
    char sserver_ip[20];
    int sserver_port;
    int num_files;
    int stosport;
    int backupport;
} StorageServer_init;

typedef struct storagelist{
    int unique_id;
    char server_ip[20];
    int server_port;
    int num_files;
    int socket;
    int stosport;
    int backupport;
    struct storagelist* next;
    struct storagelist* backup1;
    struct storagelist* backup2;
}storagelist;

typedef storagelist* sserverlist;

typedef struct stlock{
    sem_t writelock;
    sem_t readlock;
    int access;
}stlock;

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    int isEndOfWord;
    stlock file;
    sserverlist storageList;
} TrieNode;

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

typedef struct LRUCache {
    char path[MAXPATHLEN + MAXFILENAME];
    TrieNode *storage;
    struct LRUCache *next;
} LRUCache;

typedef struct async_ack{
    char client_ip[16];
    int async_port;
    char data[100];
}async_ack;

//extern
extern sserverlist sshead;
extern TrieNode *root;
extern LRUCache *cache;

//types
#define CONNECT 0
#define READ 1
#define WRITE 2
#define DELETE 3
#define GETINFO 4
#define STREAM 5
#define CREATE 6
#define COPY 7
#define LIST 8

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

// Error Codes
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

// backcommunication.c
void *handle_back_channel_communication(void *arg);

//clienthandler.c
void *clientreqs(void *arg);
void *executeclient(void *arg);
void getipandport(clientrequest *a);
void create_file(clientrequest *a);
void copyfile(clientrequest *a);
void delete_file(clientrequest *a);
void calllist(clientrequest *a);
sserverlist find_server_with_least_files();

//sserverhandler.c
void *sserverinit(void *arg);
sserverlist createnode();
void removefromsslist(int id);
void addtosslist(StorageServer_init *a,int socket);
sserverlist findsserver(int id);
void printsslist();
int pingserver(int socket);
int send_pack_to_sserver(int socket, clientrequest *a);

//trie.c
TrieNode *createNode();
void insert(TrieNode *root, char *key, int unique_id);
TrieNode *search(TrieNode *root, char *key);
void build_trie(TrieNode *root, char *file, int unique_id);
void printTrieHelper(TrieNode *root, char *buffer, int depth);
void printTrie(TrieNode *root);
void deleteFile(TrieNode *root, char *key);
void displayAllStringsHelper(TrieNode *root, char *curr_str, int depth);
void displayAllStrings(TrieNode *root);
void listAccessiblePaths(TrieNode *root, char *buffer, int depth, int client_socket, int listAll);
void copyFile(TrieNode *rootNode, char *source, char *dest);
void traverse(TrieNode *node, char *path, int depth, int id);

//logging.c 
void logs(int server, int ss_id, int ss_port_or_client_socket, int request_type, char* request_data, int error_code);

//locks.c
void initlocks();
void getssreadlock();
void getsswritelock();
void releasesswritelock();
void releasessreadlock();
void gettriereadlock();
void gettriewritelock();
void releasetriereadlock();
void releasetriewritelock();
void getlrureadlock();
void releaselrureadlock();
void getlruwritelock();
void releaselruwritelock();
void releaseloglock();
void getloglock();
int getfilereadlock(TrieNode *node);
void releasefilereadlock(TrieNode *node);
int getfilewritelock(TrieNode *node);
void releasefilewritelock(TrieNode *node);

//lru.c
TrieNode *get_from_cache(char *path);
void put_in_cache(TrieNode *toput,char *path);
LRUCache *createCache(TrieNode *toput,char *path);

#endif