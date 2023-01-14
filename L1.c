#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
int memread, memwrite, cachehit, cachemiss = 0;

typedef struct node{
    int valid, age;
    unsigned long tag;
    struct node* next;
}node;

node* createNode(){
    node* newNode = malloc(sizeof(node));
    newNode->valid = 0, newNode->tag = 0, newNode->age = 0, newNode->next = NULL;
    return newNode;
}

node* enqueue(node* root, node* newNode){
    if(root == NULL){
        root = newNode;
    }else{
        node* start = root;
        while(start->next != NULL) start = start->next;
        start->next = newNode;
    }
    return root;
}

node* moveToFront(node* root, node* n){ //moves a node to the front of the linked list if it is not already at the front
    if(root == NULL){
        root = n;
    }else if(root->tag != n->tag){
        node* curr = root;
        while(curr->next->tag != n->tag) curr = curr->next;
        curr->next = n->next;
        n->next = root;
        root = n;
    }
    return root;
}

void freeUp(node** l1, long num){ // frees up the entire l1 table
    for (int i = 0; i < num; i++){
        node* t = l1[i];
        node* p;
        while (t->next != NULL){
            p = t, t = t->next;
            free(p);
        }
        free(t);
    }
    free(l1);
}

void cache(node** l1, char operation, char l1policy[5], int* age, unsigned long setIndex, unsigned long tagBits){
    node* ptr = l1[setIndex];
    while((ptr != NULL) && (ptr->tag != tagBits)) ptr = ptr->next; //ptr is now either at the target node (hit), or is NULL (miss)
    if(ptr != NULL){ //HIT
        cachehit++;
        if(operation == 'R'){
            l1[setIndex] = moveToFront(l1[setIndex], ptr);
        }else if(operation == 'W'){
            memwrite++, l1[setIndex] = moveToFront(l1[setIndex], ptr);
        }
    }else if(ptr == NULL){ //MISS
        cachemiss++;
        ptr = l1[setIndex];
        while((ptr != NULL) && (ptr->tag != 0)) ptr = ptr->next; //pointer is now either at the first empty node or its after the end of the LL and is NULL
        if(ptr != NULL){ //not all blocks are full (a vacant node was found)
            ptr->age = age[setIndex], ptr->valid = 1, ptr->tag = tagBits;
            age[setIndex]++, l1[setIndex] = moveToFront(l1[setIndex], ptr);  //now the vacant node has been properly populated
            if(operation == 'R'){
                memread++;
            }else if(operation == 'W'){
                memread++, memwrite++;
            }
        }else if(ptr == NULL){ //all blocks are full
            if(operation == 'R'){
                memread++;
            }else if(operation == 'W'){
                memread++, memwrite++;
            }
            if(strcmp(l1policy, "lru") == 0){
                ptr = l1[setIndex];
                while(ptr->next != NULL) ptr = ptr->next;
                ptr->age = age[setIndex], ptr->tag = tagBits, ptr->valid = 1;
                age[setIndex]++, l1[setIndex] = moveToFront(l1[setIndex], ptr);
            }else if(strcmp(l1policy, "fifo") == 0){
                ptr = l1[setIndex];
                int min = l1[setIndex]->age;
                while(ptr != NULL){    
                    if(ptr->age < min) min = ptr->age;
                    ptr = ptr->next; 
                } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                ptr = l1[setIndex];
                while(ptr->age != min) ptr = ptr->next; //now ptr is at the oldest node
                ptr->age = age[setIndex], ptr->tag = tagBits, ptr->valid = 1;
                age[setIndex]++, l1[setIndex] = moveToFront(l1[setIndex], ptr);
            }
        }
    }
}

int main(int argc, char* argv[argc+1]){
    unsigned int l1cacheSize = atoi(argv[1]); //l1 cache size
    char *pos = argv[2];
    unsigned int l1associativity;
    while(*pos != ':' && *pos != '\0')  pos++;
    pos++;  // Skip the ':'
    l1associativity = atoi(pos); //associativity

    char l1policy[5];
    strcpy(l1policy, argv[3]); //l1 policy
    int blockSize = atoi(argv[4]), numSets = l1cacheSize/(blockSize*l1associativity), offsetSize = log2(blockSize), indexSize = log2(numSets);

    node **l1 = malloc(sizeof(node *)*numSets); //creating the l1 table by allocating space for 'S' node pointers
    for(int i = 0; i < numSets; i++) {
        l1[i] = NULL;  // Set to null, because malloc() won't.
        for(int j = 0; j < l1associativity; j++) {
            node* newNode = createNode();
            l1[i] = enqueue(l1[i], newNode);
        }
    }
    int *age = malloc(numSets * sizeof(int)); //creating an array of length numSets with an age in each index
    for(int l=0; l<numSets; l++) age[l] = 1;

    FILE * fp = fopen(argv[5], "r");
    char operation;
    unsigned long address;
    if(fp == NULL){
        return 1;
    }else{
        while(fscanf(fp, "%c %lx\n", &operation, &address) != EOF){
            unsigned long tag = address >> (offsetSize + indexSize), setIndex = (address >> offsetSize) & (numSets - 1);
            cache(l1, operation, l1policy, age, setIndex, tag);
        }
    }
    printf("memread:%d\nmemwrite:%d\ncachehit:%d\ncachemiss:%d\n", memread, memwrite, cachehit, cachemiss);
    freeUp(l1, numSets), free(age);
    return 0;
}
