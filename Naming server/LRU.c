#include"headers.h"
int currentcachecount=0;
TrieNode *get_from_cache(char *path){
    getlrureadlock();
    LRUCache *node=cache;
    while (node) {
        if (strcmp(node->path, path) == 0) {
            printf(GREEN("Cache hit: %s\n"), path);
            releaselrureadlock();
            return node->storage;
        }
        node = node->next;
    }
    printf(RED("Cache miss: %s\n"), path);
    releaselrureadlock();
    TrieNode *response = search(root,path);
    if(response==NULL){
        return NULL;
    }
    else{
        put_in_cache(response,path);
    }
    return response;
}
LRUCache *createCache(TrieNode *toput,char *path){
    LRUCache *temp=NULL;
    temp=(LRUCache *)malloc(sizeof(LRUCache));
    temp->next=NULL;
    strcpy(temp->path,path);
    temp->storage=toput;
    return temp;
}
void put_in_cache(TrieNode *toput,char *path) {
    getlruwritelock();
    if(currentcachecount>=CACHE_SIZE){
        LRUCache *temp=cache;
        cache=cache->next;
        printf(RED("Evicted from cache: %s\n"), temp->path);
        free(temp);
        currentcachecount--;
    }
    if(cache==NULL){
        currentcachecount++;
        cache=createCache(toput,path);
        printf(CYAN("Inserted into cache: %s\n"), path);
        releaselruwritelock();
        return;
    }
    currentcachecount++;
    LRUCache *head=cache;
    while(head->next!=NULL){
        head=head->next;
    }
    head->next=createCache(toput,path);
    releaselruwritelock();
    printf(CYAN("Inserted into cache: %s\n"), path);
}