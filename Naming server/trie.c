#include "headers.h"

TrieNode *createNode() {
    TrieNode *newNode = (TrieNode *)malloc(sizeof(TrieNode));
    newNode->isEndOfWord = 0;
    newNode->storageList = NULL;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        newNode->children[i] = NULL;
    }
    sem_init(&newNode->file.writelock, 0, 1);
    sem_init(&newNode->file.readlock, 0, 1);
    newNode->file.access=0;
    return newNode;
}

void insert(TrieNode *rootNode, char *key, int unique_id) {
    gettriewritelock();
    if(strcmp(key,".") == 0){
        releasetriewritelock();
        return;
    }

    TrieNode *node = rootNode;
    for (int i = 0; key[i]; i++) {
        if(i == 0 && (key[i] == '.' || key[i] == '/')) continue;
        int index = key[i];
        if (!node->children[index]) {
            node->children[index] = createNode();
        }
        node = node->children[index];
    }
    sserverlist x = findsserver(unique_id);
    if(node->isEndOfWord == 0) x->num_files++;
    node->isEndOfWord = 1;
    node->storageList = x;

    releasetriewritelock();
}

TrieNode *search(TrieNode *rootNode, char *key){
    gettriereadlock();

    if(strcmp(key,".") == 0){
        releasetriereadlock();
        return rootNode;
    }

    TrieNode *node = rootNode;
    for (int i = 0; key[i]; i++) {
        if(i == 0 && key[i] == '.') continue;
        int index = key[i];
        if (!node->children[index]) {
            releasetriereadlock();
            return 0;
        }
        node = node->children[index];
    }
    if(node != NULL && node->isEndOfWord){
        releasetriereadlock();
        return node;
    }
    releasetriereadlock();
    return NULL;
}

// Function to build the trie
void build_trie(TrieNode *rootNode, char *files, int unique_id) {
    // printf("%s\n",files);
    insert(rootNode, files, unique_id);
}

void free_trie(TrieNode *node) {
    if (node == NULL) {
        return;
    }

    // Recursively free all children
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i] != NULL) {
            free_trie(node->children[i]);
        }
    }

    if (node->isEndOfWord && node->storageList) {
        node->storageList->num_files--;
    }

    node->isEndOfWord = 0;
    node->storageList = NULL;

    // Free the current node
    free(node);
}

void deleteHelper(TrieNode *rootNode, char *key, int depth) {
    if (!rootNode) return;
    
    if (depth == strlen(key)) {
        
        if (rootNode->isEndOfWord && rootNode->storageList) {
            rootNode->storageList->num_files--;
        }

        rootNode->isEndOfWord = 0;
        free_trie(rootNode->children[47]);
        rootNode->children[47] = NULL;
        return;
    }

    int index = key[depth];
    if(depth == 0 && key[0] == '.') deleteHelper(rootNode, key, depth + 1);
    else deleteHelper(rootNode->children[index], key, depth + 1);

    // Check if the node has no children and is not end of word, then free it
    int hasChildren = 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (rootNode->children[i] != NULL) {
            hasChildren = 1;
            break;
        }
    }
    if (!hasChildren && !rootNode->isEndOfWord) {
        free(rootNode);
        rootNode = NULL;
    }
}

void deleteFile(TrieNode *rootNode, char *key) {
    // printf("%s\n",key);
    gettriewritelock();
    deleteHelper(rootNode, key, 0);
    releasetriewritelock();
}

void displayAllStringsHelper(TrieNode *rootNode, char *curr_str, int depth) {
    if (rootNode->isEndOfWord) {
        printf("server:%s id:%d",rootNode->storageList->server_ip,rootNode->storageList->server_port);
        printf("%s\n", curr_str);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (rootNode->children[i]) {
            curr_str[depth] = (char)i;
            curr_str[depth + 1] = '\0';
            displayAllStringsHelper(rootNode->children[i], curr_str, depth + 1);
        }
    }
}

void displayAllStrings(TrieNode *rootNode) {
    gettriereadlock();
    char curr_str[1024] = {0};
    displayAllStringsHelper(rootNode, curr_str, 0);
    printf("SERVER %d SIZE %d",rootNode->storageList->unique_id,rootNode->storageList->num_files);
    releasetriereadlock();
}

void listAccessiblePaths(TrieNode *node, char *buffer, int depth, int client_socket, int listAll) {
    if (!node) return;
    // If the current node is an end-of-word and we are listing all files, send the path
    if (node->isEndOfWord && listAll) {
        buffer[depth] = '\0'; // Null-terminate the current path
         if (strlen(buffer) > 0) {
            send(client_socket, buffer, strlen(buffer), 0);
            send(client_socket, "\n", 1, 0);
            printf("%s\n", buffer);
        }
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            buffer[depth] = (char)i; // Store the character corresponding to the child
            if (!listAll && node->children[i]->isEndOfWord) {
                buffer[depth + 1] = '\0'; // Null-terminate the current path
                 if (strlen(buffer) > 0) {
                    printf("%s\n", buffer);
                    send(client_socket, buffer, strlen(buffer), 0);
                    send(client_socket, "\n", 1, 0);
                }
            } else {
                listAccessiblePaths(node->children[i], buffer, depth + 1, client_socket, listAll);
            }
        }
    }
}

void copyFile(TrieNode *rootNode, char *source, char *dest) {
    gettriereadlock();
    TrieNode *sourceNode = search(rootNode, source);
    TrieNode *destNode = search(rootNode, dest);
    releasetriereadlock();

    if (!sourceNode) {
        printf("Source path not found\n");
        return;
    }
    
    if (!destNode) {
        printf("Destination path not found\n");
        return;
    }

    char newPath[1024] = {0};
    strcpy(newPath, dest);
    if(strcmp(dest,".")) strcat(newPath, "/");
    
    // Add the source directory name to the path
    
    char *lastSlash = strrchr(source, '/');
    char *sourceDirName = lastSlash ? lastSlash + 1 : source;
    strcat(newPath, sourceDirName);
    
    newPath[strlen(newPath)] = '\0';
    insert(rootNode, newPath, destNode->storageList->unique_id);

    printf("NEWPATH %s\n", newPath);

    newPath[strlen(newPath)] = '/';
    printf("NEWPATH %s\n", newPath);
    traverse(sourceNode->children[47], newPath, strlen(newPath), destNode->storageList->unique_id);
}

void traverse(TrieNode *node, char *path, int depth, int id) {

    printf("CAME HERE\n");
    if(!node) return;

    printf("DEPTH %d\n",depth);

    if (node->isEndOfWord) {
        path[depth] = '\0';
        printf("NEW WORD FOUND %s\n",path);
        insert(root, path,id);
    }
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            path[depth] = (char)i;
            traverse(node->children[i], path, depth + 1,id);
        }
    }
}