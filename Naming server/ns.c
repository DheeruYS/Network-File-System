#include "headers.h"
TrieNode *root;
sserverlist sshead;
LRUCache *cache;
void initns(){
    initlocks();
    signal(SIGPIPE, SIG_IGN);
    cache=NULL;
    sshead=NULL;
    root=NULL;
}

int main(){
    initns();
    root=createNode();
    pthread_t client;
    pthread_t sserver;
    pthread_t back_channel_thread;
    pthread_create(&client,NULL,&clientreqs,NULL);
    pthread_create(&sserver,NULL,&sserverinit,NULL);
    pthread_create(&back_channel_thread, NULL, &handle_back_channel_communication, NULL); // Start back channel listener
    pthread_join(client,NULL);
    pthread_join(sserver,NULL);
    pthread_join(back_channel_thread, NULL);
    return 0;
}