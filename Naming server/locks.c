#include "headers.h"
stlock sslock;
stlock trielock;
stlock lrulock;
sem_t loglock;

void initlocks(){
    sem_init(&sslock.writelock, 0, 1);
    sem_init(&sslock.readlock, 0, 1);
    sem_init(&trielock.writelock, 0, 1);
    sem_init(&trielock.readlock, 0, 1);
    sem_init(&lrulock.writelock, 0, 1);
    sem_init(&lrulock.readlock, 0, 1);
    sem_init(&loglock, 0, 1);
    sslock.access=0;
    trielock.access=0;
    lrulock.access=0;
}
void getssreadlock(){
    sem_wait(&sslock.readlock);
    sslock.access++;
    if(sslock.access==1){
        sem_wait(&sslock.writelock);
    }
    sem_post(&sslock.readlock);
}
void releasessreadlock(){
    sem_wait(&sslock.readlock);
    sslock.access--;
    if(sslock.access==0){
        sem_post(&sslock.writelock);
    }
    sem_post(&sslock.readlock);
}
void getsswritelock(){
    sem_wait(&sslock.writelock);
}
void releasesswritelock(){
    sem_post(&sslock.writelock);
}
void gettriereadlock(){
    sem_wait(&trielock.readlock);
    trielock.access++;
    if(trielock.access==1){
        sem_wait(&trielock.writelock);
    }
    sem_post(&trielock.readlock);
}
void releasetriereadlock(){
    sem_wait(&trielock.readlock);
    trielock.access--;
    if(trielock.access==0){
        sem_post(&trielock.writelock);
    }
    sem_post(&trielock.readlock);
}
void gettriewritelock(){
    sem_wait(&trielock.writelock);
}
void releasetriewritelock(){
    sem_post(&trielock.writelock);
}
void getlrureadlock(){
    sem_wait(&lrulock.readlock);
    lrulock.access++;
    if(lrulock.access==1){
        sem_wait(&lrulock.writelock);
    }
    sem_post(&lrulock.readlock);
}
void releaselrureadlock(){
    sem_wait(&lrulock.readlock);
    lrulock.access--;
    if(lrulock.access==0){
        sem_post(&lrulock.writelock);
    }
    sem_post(&lrulock.readlock);
}
void getlruwritelock(){
    sem_wait(&lrulock.writelock);
}
void releaselruwritelock(){
    sem_post(&lrulock.writelock);
}
void getloglock(){
    sem_wait(&loglock);
}
void releaseloglock(){
    sem_post(&loglock);
}
int getfilereadlock(TrieNode *node){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 10;
    // sem_wait(&node->file.readlock);
    if(sem_timedwait(&node->file.readlock,&ts)==-1){
        return 0;
    }
    node->file.access++;
    if(node->file.access==1){
        if(sem_timedwait(&node->file.writelock,&ts)==-1){
            sem_post(&node->file.readlock);
            return 0;
        }
    }
    sem_post(&node->file.readlock);
    return 1;
}
void releasefilereadlock(TrieNode *node){
    sem_wait(&node->file.readlock);
    node->file.access--;
    if(node->file.access==0){
        sem_post(&node->file.writelock);
    }
    sem_post(&node->file.readlock);
}
int getfilewritelock(TrieNode *node){
    printf("here");
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 10;
    if(sem_timedwait(&node->file.writelock,&ts)==-1){
        return 0;
    }
    return 1;
}
void releasefilewritelock(TrieNode *node){
    sem_post(&node->file.writelock);
}