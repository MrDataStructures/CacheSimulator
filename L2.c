#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
int memread, memwrite, l1cachehit, l1cachemiss, l2cachehit, l2cachemiss = 0;

typedef struct node{
    int valid, age;
    unsigned long l1tag;
    unsigned long l2tag;
    unsigned long l2Index;
    struct node* next;
}node;

node* createNode(){
    node* newNode = malloc(sizeof(node));
    newNode->valid = 0;
    newNode->l1tag = 0;
    newNode->l2tag = 0;
    newNode->age = 0;
    newNode->l2Index = -1;
    newNode->next = NULL;
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

void printHash(node** hash, int S){
    for(long i=0; i < S; i++){
        node* current = hash[i];
        while(current != NULL){
            printf("%lx ", current->l1tag);
            current = current->next;
        }
        printf("\n");
    }
}

void l2printHash(node** hash, int S){
    for(long i=0; i < S; i++){
        node* current = hash[i];
        while(current != NULL){
            printf("%lx ", current->l2tag);
            current = current->next;
        }
        printf("\n");
    }
}

node* moveToFront(node* root, node* n){ //moves a node to the front of the linked list if it is not already at the front
    if(root == NULL){
        root = n;
    }else if(root->l1tag != n->l1tag){
        node* curr = root;
        while(curr->next->l1tag != n->l1tag) curr = curr->next;
        curr->next = n->next;
        n->next = root;
        root = n;
    }
    return root;
}

void freeUp(node** hash, long num){ // frees up the entire hash
    for (int i = 0; i < num; i++){
        node* t = hash[i];
        node* p;
        while (t->next != NULL){
            p = t;
            t = t->next;
            free(p);
        }
        free(t);
    }
    free(hash);
}

void cache(node** l1hash, node** l2hash, char operation, char l1policy[5], char l2policy[5], unsigned long address, int* l1age, int* l2age, unsigned long l1setIndex, unsigned long l1tagBits, unsigned long l2setIndex, unsigned long l2tagBits){
    node* ptr = l1hash[l1setIndex];
    while((ptr != NULL) && (ptr->l1tag != l1tagBits)) ptr = ptr->next; //at this point, ptr is either at the target node (hit), or the node does not exist and ptr is NULL and is after the end of the linked list (miss).                                    
    if(ptr != NULL){ // L1 HIT
        l1cachehit++;
        if(operation == 'R'){
            l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], ptr);
        }else if(operation == 'W'){
            memwrite++;
            l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], ptr);
        }
    }else if(ptr == NULL){ // L1 MISS
        l1cachemiss++;
        node* t = l2hash[l2setIndex];
        while((t != NULL) && (t->l2tag != l2tagBits)) t = t->next; //t is either now NULL at target node in L2 (HIT), or it is NULL (miss)
        if(t != NULL){ // L2 HIT (t is now at the target node in L2)
            l2cachehit++;  //now we need to bring it to L1.

            if(operation == 'R'){
                memread = memread + 0;
            }else if(operation == 'W'){
                memwrite++;
            }

            unsigned long otag = t->l1tag;
            unsigned long ttag = t->l2tag; //evicted data from l2
            unsigned long twoIndex = t->l2Index;

            t->age = 0;
            t->l1tag = 0;
            t->l2tag = 0;
            t->valid = 0;
            t->l2Index = -1;
            node* L = l1hash[l1setIndex];
            while((L != NULL) && (L->l1tag != 0)){ //L is now either at the first empty node or its after the end of the LL and is NULL
                L = L->next;
            }
            if(L != NULL){ // a vacant node in L1 was found
                L->l1tag = otag;
                L->l2tag = ttag;
                L->age = l1age[l1setIndex];
                l1age[l1setIndex]++;
                L->valid = 1;
                L->l2Index = twoIndex;
                l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], L);
            }else if(L == NULL){ // all blocks are full in L1
                if(strcmp(l1policy, "fifo") == 0){
                    node* start = l1hash[l1setIndex];
                    int min = l1hash[l1setIndex]->age;
                    while(start != NULL){    
                        if(start->age < min) min = start->age;
                        start = start->next; 
                    } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                    start = l1hash[l1setIndex];
                    while(start->age != min) start = start->next; //now start is at the oldest node
                    unsigned long onetag = start->l1tag; //temporary data holder for the evicted node's l1 tag
                    unsigned long twotag = start->l2tag; //temporary data holder for the evicted node's l2 tag
                    unsigned long sID = start->l2Index;  //temporary data holder for the evicted nodes's l2 index

                    start->l1tag = otag;
                    start->l2tag = ttag;
                    start->age = l1age[l1setIndex];  // now i populated the fifo evicted node in L1
                    l1age[l1setIndex]++;
                    start->valid = 1;
                    start->l2Index = twoIndex;
                    l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], start);

                    node* curr = l2hash[sID];
                    while((curr != NULL) && (curr->l2tag != 0)){ //curr is now either at the first empty node or its after the end of the LL and is NULL
                        curr = curr->next;
                    }
                    if(curr != NULL){  //a vacant node was found in L2 for the evicted one in L1
                        curr->age = l2age[sID];
                        l2age[sID]++;
                        curr->l1tag = onetag;
                        curr->l2tag = twotag;
                        curr->l2Index = sID;
                        curr->valid = 1;
                        l2hash[sID] = moveToFront(l2hash[sID], curr);
                    }else if(curr == NULL){ // L2 is full
                        if(strcmp(l2policy, "fifo") == 0){
                            node* tracker = l2hash[sID];
                            int min = l1hash[sID]->age;
                            while(tracker != NULL){    
                                if(tracker->age < min){
                                    min = tracker->age;
                                }
                                tracker = tracker->next; 
                            } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                            tracker = l2hash[sID];
                            while(tracker->age != min){
                                tracker = tracker->next;
                            } //now tracker is at the oldest node
                            tracker->age = l2age[sID];
                            l2age[sID]++;
                            tracker->l1tag = onetag;
                            tracker->l2tag = twotag;
                            tracker->l2Index = sID;
                            tracker->valid = 1;
                            l2hash[sID] = moveToFront(l2hash[sID], tracker);
                        }else if(strcmp(l2policy, "lru") == 0){
                            node* cur = l2hash[sID];
                            while(cur->next != NULL){
                                cur = cur->next; 
                            }
                            cur->age = l2age[sID];
                            l2age[sID]++;
                            cur->l1tag = onetag;
                            cur->l2tag = twotag;
                            cur->l2Index = sID;
                            cur->valid = 1;
                            l2hash[sID] = moveToFront(l2hash[sID], cur);
                        }
                    }
                }else if(strcmp(l1policy, "lru") == 0){   // if we are trying to bring the node to L1 but the blocks are full
                    node* last = l1hash[l1setIndex];
                    while(last->next != NULL){
                        last = last->next; 
                    }

                    unsigned long one = last->l1tag;
                    unsigned long two = last->l2tag;   //evicted data
                    unsigned long secondID = last->l2Index;
                    
                    last->age = l1age[l1setIndex];
                    l1age[l1setIndex]++;
                    last->l1tag = otag;
                    last->l2tag = ttag;
                    last->l2Index = twoIndex;
                    last->valid = 1;
                    l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], last);

                    node* position = l2hash[secondID];
                    while((position != NULL) && (position->l2tag != 0)){ //position is now either at the first empty node or its after the end of the LL and is NULL
                        position = position->next;
                    }
                    if(position != NULL){  //a vacant node was found in L2 for the evicted one in L1
                        position->age = l2age[secondID];
                        l2age[secondID]++;
                        position->l1tag = one;
                        position->l2tag = two;
                        position->l2Index = secondID;
                        position->valid = 1;
                        l2hash[secondID] = moveToFront(l2hash[secondID], position);
                    }else if(position == NULL){ // L2 is full
                        if(strcmp(l2policy, "fifo") == 0){
                            node* tr = l2hash[secondID];
                            int min = l2hash[secondID]->age;
                            while(tr != NULL){    
                                if(tr->age < min){
                                    min = tr->age;
                                }
                                tr = tr->next; 
                            } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                            tr = l2hash[secondID];
                            while(tr->age != min){
                                tr = tr->next;
                            } //now tr is at the oldest node
                            tr->age = l2age[secondID];
                            l2age[secondID]++;
                            tr->l1tag = one;
                            tr->l2tag = two;
                            tr->l2Index = secondID;
                            tr->valid = 1;
                            l2hash[secondID] = moveToFront(l2hash[secondID], tr);
                        }else if(strcmp(l2policy, "lru") == 0){
                            node* tra = l2hash[secondID];
                            while(tra->next != NULL){
                                tra = tra->next; 
                            }
                            tra->age = l2age[secondID];
                            l2age[secondID]++;
                            tra->l1tag = one;
                            tra->l2tag = two;
                            tra->l2Index = secondID;
                            tra->valid = 1;
                            l2hash[secondID] = moveToFront(l2hash[secondID], tra);
                        }
                    }
                }
            }
        }else if(t == NULL){ // L2 MISS
            l2cachemiss++;
            if(operation == 'R'){
                memread = memread + 1;
            }else if(operation == 'W'){
                memread = memread + 1;
                memwrite = memwrite + 1;
            }
            node* first = l1hash[l1setIndex];
            while((first != NULL) && (first->l1tag != 0)){ //first is now either at the first empty node or its after the end of the LL and is NULL
                first = first->next;
            }
            if(first != NULL){ //not all blocks are full (a vacant node was found)
                first->age = l1age[l1setIndex];
                l1age[l1setIndex]++;
                first->valid = 1;
                first->l1tag = l1tagBits;
                first->l2tag = l2tagBits;
                first->l2Index = l2setIndex;
                l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], first);  //now the vacant node has been properly populated and moved to the front
            }else if(first == NULL){ //l1 is full
                if(strcmp(l1policy, "fifo") == 0){
                    node* beg = l1hash[l1setIndex];
                    int smallest = l1hash[l1setIndex]->age;
                    while(beg != NULL){    
                        if(beg->age < smallest){
                            smallest = beg->age;
                        }
                        beg = beg->next; 
                    } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                    beg = l1hash[l1setIndex];
                    while(beg->age != smallest){
                        beg = beg->next;
                    } //now beg is at the oldest node
                    unsigned long data1 = beg->l1tag;
                    unsigned long data2 = beg->l2tag;   //evicted data
                    unsigned long data2ID = beg->l2Index;
                    beg->age = l1age[l1setIndex];
                    l1age[l1setIndex]++;
                    beg->l1tag = l1tagBits;
                    beg->l2tag = l2tagBits;
                    beg->l2Index = l2setIndex;
                    beg->valid = 1;
                    l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], beg);

                    node* place = l2hash[data2ID];
                    while((place != NULL) && (place->l2tag != 0)){ //place is now either at the first empty node or its after the end of the LL and is NULL
                        place = place->next;
                    }
                    if(place != NULL){  //a vacant node was found in L2 for the evicted one in L1
                        place->age = l2age[data2ID];
                        l2age[l2setIndex]++;
                        place->l1tag = data1;
                        place->l2tag = data2;
                        place->l2Index = data2ID;
                        place->valid = 1;
                        l2hash[data2ID] = moveToFront(l2hash[data2ID], place);
                    }else if(place == NULL){ // L2 is full
                        if(strcmp(l2policy, "fifo") == 0){
                            node* spot = l2hash[data2ID];
                            int tiny = l2hash[data2ID]->age;
                            while(spot != NULL){    
                                if(spot->age < tiny){
                                    tiny = spot->age;
                                }
                                spot = spot->next; 
                            } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                            spot = l2hash[data2ID];
                            while(spot->age != tiny){
                                spot = spot->next;
                            } //now spot is at the oldest node
                            spot->age = l2age[data2ID];
                            l2age[l2setIndex]++;
                            spot->l1tag = data1;
                            spot->l2tag = data2;
                            spot->l2Index = data2ID;
                            spot->valid = 1;
                            l2hash[data2ID] = moveToFront(l2hash[data2ID], spot);
                        }else if(strcmp(l2policy, "lru") == 0){
                            node* po = l2hash[data2ID];
                            while(po->next != NULL){
                                po = po->next; 
                            }
                            po->age = l2age[data2ID];
                            l2age[data2ID]++;
                            po->l1tag = data1;
                            po->l2tag = data2;
                            po->l2Index = data2ID;
                            po->valid = 1;
                            l2hash[data2ID] = moveToFront(l2hash[data2ID], po);
                        }
                    }
                }else if(strcmp(l1policy, "lru") == 0){ // l1 miss and l2 miss and now try to put it in l1 but it is full and policy is lru
                    node* st = l1hash[l1setIndex];
                    while(st->next != NULL){
                        st = st->next;   //st is now at the last node in the set
                    }
                    unsigned long firstTag = st->l1tag;
                    unsigned long secondTag = st->l2tag;   //evicted data
                    unsigned long secID = st->l2Index;

                    st->age = l1age[l1setIndex];
                    l1age[l1setIndex]++;
                    st->l1tag = l1tagBits;
                    st->l2tag = l2tagBits;
                    st->l2Index = l2setIndex;
                    st->valid = 1;
                    l1hash[l1setIndex] = moveToFront(l1hash[l1setIndex], st);
                    
                    node* area = l2hash[secID];
                    while((area != NULL) && (area->l2tag != 0)){ //place is now either at the first empty node or its after the end of the LL and is NULL
                        area = area->next;
                    }
                    if(area != NULL){  //a vacant node was found in L2 for the evicted one in L1
                        area->age = l2age[secID];
                        l2age[secID]++;
                        area->l1tag = firstTag;
                        area->l2tag = secondTag;
                        area->l2Index = secID;
                        area->valid = 1;
                        l2hash[secID] = moveToFront(l2hash[secID], area);
                    }else if(area == NULL){ // L2 is full
                        if(strcmp(l2policy, "fifo") == 0){
                            node* ps = l2hash[secID];
                            int smal = l2hash[secID]->age;
                            while(ps != NULL){    
                                if(ps->age < smal){
                                    smal = ps->age;
                                }
                                ps = ps->next; 
                            } //now we have the smallest age. we will now traverse through again until we find the node with that age and evict it
                            ps = l2hash[secID];
                            while(ps->age != smal){
                                ps = ps->next;
                            } //now ps is at the oldest node
                            ps->age = l2age[secID];
                            l2age[secID]++;
                            ps->l1tag = firstTag;
                            ps->l2tag = secondTag;
                            ps->l2Index = secID;
                            ps->valid = 1;
                            l2hash[secID] = moveToFront(l2hash[secID], ps);
                        }else if(strcmp(l2policy, "lru") == 0){
                            node* pl = l2hash[secID];
                            while(pl->next != NULL){
                                pl = pl->next; 
                            }
                            pl->age = l2age[secID];
                            l2age[secID]++;
                            pl->l1tag = firstTag;
                            pl->l2tag = secondTag;
                            pl->l2Index = secID;
                            pl->valid = 1;
                            l2hash[secID] = moveToFront(l2hash[secID], pl);
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[argc+1]){
    unsigned int l1cacheSize = atoi(argv[1]);
    char *pos = argv[2];
    unsigned int l1associativity;
    while(*pos != ':' && *pos != '\0')  pos++;
    pos++;  // Skip the ':'
    l1associativity = atoi(pos); //l1 associativity

    char l1policy[5];
    strcpy(l1policy, argv[3]); //l1 policy

    double blockSize = atoi(argv[4]); //block size for l1 and l2
    int l1numSets = l1cacheSize/(blockSize*l1associativity); //number of sets for l1
    int offsetSize = log2(blockSize); //offset size for l1 and l2
    int l1indexSize = log2(l1numSets); //index size for l1

    char l2policy[5];
    strcpy(l2policy, argv[7]); //l2 policy

    unsigned int l2cacheSize = atoi(argv[5]); //cache size for l2
    char *pt = argv[6];
    unsigned int l2associativity;
    while(*pt != ':' && *pt != '\0')  pt++;
    pt++;  // Skip the ':'
    l2associativity = atoi(pt); //l2 associativity

    node **hash = malloc(sizeof(node *)*l1numSets); //creating the l1 hash table by allocating space for 'S' node pointers
    for(int i = 0; i < l1numSets; i++) {
        hash[i] = NULL;  // Set to null, because malloc() won't.
        for(int j = 0; j < l1associativity; j++) {
            node* newNode = createNode();
            hash[i] = enqueue(hash[i], newNode);
        }
    }
    int *l1age = malloc(l1numSets * sizeof(int)); //for l1, creating an array of length numSets with an age in each index
    for(int l=0; l<l1numSets; l++) l1age[l] = 1;

    int l2numSets = l2cacheSize/(blockSize*l2associativity); //number of sets for l2
    int l2indexSize = log2(l2numSets);
    node **l2hash = malloc(sizeof(node *)*l2numSets);
    for(int o=0; o < l2numSets; o++){
        l2hash[o] = NULL;
        for(int t=0; t < l2associativity; t++){
            node* newN = createNode();
            l2hash[o] = enqueue(l2hash[o], newN);
        }
    }
    int *l2age = malloc(l2numSets * sizeof(int)); //for l2, creating an array of length numSets with an age in each index
    for(int s=0; s<l2numSets; s++) l1age[s] = 1;

    FILE * fp = fopen(argv[8], "r");
    char operation;
    unsigned long address;
    if(fp == NULL){
        return 1;
    }else{
        while(fscanf(fp, "%c %lx\n", &operation, &address) != EOF){
            unsigned long l1tag = address >> (offsetSize + l1indexSize);
            unsigned long l1setIndex = (address >> offsetSize) & (l1numSets - 1);
            
            unsigned long l2tag = address >> (offsetSize + l2indexSize);
            unsigned long l2setIndex = (address >> offsetSize) & (l2numSets - 1);
            cache(hash, l2hash, operation, l1policy, l2policy, address, l1age, l2age, l1setIndex, l1tag, l2setIndex, l2tag);
            //printf("l1tag is: %lx l1 setIndex is: %li l2tag is: %lx l2 setIndex is: %li\n", l1tag, l1setIndex, l2tag, l2setIndex);
        }
    } //./second 32 assoc:2 fifo 4 64 assoc:16 lru trace2.txt
    printf("memread:%d\nmemwrite:%d\nl1cachehit:%d\nl1cachemiss:%d\nl2cachehit:%d\nl2cachemiss:%d\n", memread, memwrite, l1cachehit, l1cachemiss, l2cachehit, l2cachemiss);
    //printf("l1 cache size:%i\nl1 assoc:%i\nl1 policy:%s\nblocksize:%lf\nl2 cache size:%i\nl2 assoc:%i\nl2 policy:%s\n", l1cacheSize, l1associativity, l1policy, blockSize, l2cacheSize, l2associativity, l2policy);
    printf("\n");
    printHash(hash, l1numSets);
    printf("\n");
    l2printHash(l2hash, l2numSets);
    freeUp(hash, l1numSets);
    freeUp(l2hash, l2numSets);
    free(l1age);
    free(l2age);
    return 0;
}
