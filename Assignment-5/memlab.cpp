#include <pthread.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[SUCCESS] " msg " \033[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\033[1;32m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define PROMPT(msg, ...) printf("\033[1;33m[PROMPT]" msg "\033[0m", ##__VA_ARGS__);

typedef unsigned int u_int;

const int MAX_PT_ENTRIES = 1024;
const int MAX_STACK_SIZE = 1024;

enum VarType {
    PRIMITIVE,
    ARRAY
};

enum DataType {
    INT,
    CHAR,
    MEDIUM_INT,
    BOOLEAN
};

// always make const type object for this
struct MyType {
    int ind;
    VarType var_type;
    DataType data_type;
    size_t len;

    MyType(int _ind, VarType _var_type, DataType _data_type, size_t _len) : ind(_ind), var_type(_var_type), data_type(_data_type), len(_len) {}
};

struct Memory {
    int *start;
    int *end;
    size_t size;
    pthread_mutex_t mutex;

    int init(size_t bytes) {
        start = (int *)malloc(bytes);
        if (start == NULL) {
            ERROR("malloc failed");
            return -1;
        }
        end = start + (bytes >> 2);
        size = bytes >> 2;  // in words

        // set up initial free list
        *start = (bytes >> 2) << 1;
        *(start + (bytes >> 2) - 1) = (bytes >> 2) << 1;

        return 0;
    }

    int *findFreeBlock(size_t sz) {  // sz is the size required for the data (in words)
        int *p = start;
        while ((p < end) && ((*p & 1) || ((*p >> 1) < sz + 2))) {
            p = p + (*p >> 1);
        }
        if (p < end) {
            return p;
        } else {
            return NULL;
        }
    }

    void allocateBlock(int *p, size_t sz) {  // sz is the size required for the data (in words)
        sz += 2;
        int old_size = *p >> 1;         // mask out low bit
        *p = (sz << 1) | 1;             // set new length and allocated bit for header
        *(p + sz - 1) = (sz << 1) | 1;  // same for footer

        if (sz < old_size) {
            *(p + sz) = (old_size - sz) << 1;            // set length in remaining for header
            *(p + old_size - 1) = (old_size - sz) << 1;  // same for footer
        }
    }

    void freeBlock(int *p) {
        *p = *p & -2;  // clear allocated flag in header
        int curr_size = *p >> 1;
        *(p + curr_size - 1) = *(p + curr_size - 1) & -2;  // clear allocated flag in footer

        int *next = p + curr_size;                // find next block
        if ((next != end) && (*next & 1) == 0) {  // if next block is free
            int next_size = *next >> 1;
            *p = (curr_size + next_size) << 1;                                // merge with next block
            *(p + curr_size + next_size - 1) = (curr_size + next_size) << 1;  // set length in footer
            curr_size += next_size;
        }

        if ((p != start) && (*(p - 1) & 1) == 0) {  // if previous block is free
            int prev_size = *(p - 1) >> 1;
            // *(p - 1) = (prev_size + curr_size) << 1;
            *(p - prev_size) = (prev_size + curr_size) << 1;      // set length in header of prev
            *(p + curr_size - 1) = (prev_size + curr_size) << 1;  // set length in footer
        }
    }
};

struct PageTableEntry {
    u_int addr : 30;
    u_int valid : 1;
    u_int marked : 1;

    void init() {
        addr = 0;
        valid = 0;
        marked = 0;
    }

    void print() {
        printf("%8d, %3d, %3d\n", addr, valid, marked);
    }
};

struct PageTable {
    PageTableEntry pt[MAX_PT_ENTRIES];
    u_int head, tail;
    size_t size;
    pthread_mutex_t mutex;
    /*
        remove from head
        add free block to tail
    */

    void init() {
        for (int i = 0; i < MAX_PT_ENTRIES; i++) {
            pt[i].init();
            pt[i].addr = (i + 1) % MAX_PT_ENTRIES;
        }
        head = 0;
        tail = MAX_PT_ENTRIES - 1;
        size = 0;
        pthread_mutex_init(&mutex, NULL);
    }

    int insert(u_int addr) {
        pthread_mutex_lock(&mutex);
        if (size == MAX_PT_ENTRIES) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        u_int next = pt[head].addr;
        pt[head].addr = addr;
        pt[head].valid = 1;
        head = next;
        size++;
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    int remove(u_int idx) {
        pthread_mutex_lock(&mutex);
        if (size == 0) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        pt[idx].valid = 0;
        pt[tail].addr = idx;
        tail = idx;
        size--;
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    void print() {
        printf("\nPage Table:\n");
        printf("Entry, Valid, Marked\n");
        for (int i = 0; i < size; i++) {
            pt[i].print();
        }
    }
};

struct Stack {
    int st[MAX_STACK_SIZE];
    int size;

    void init() {
        size = 0;
    }

    int top() {
        return st[size - 1];
    }

    void push(int v) {
        st[size++] = v;
    }

    int pop() {
        return st[--size];
    }
};

// global variables
Memory *mem;
PageTable *page_table;
Stack *var_stack;

int getSize(DataType data_type) {
    if (data_type == INT) {
        return 32;
    } else if (data_type == CHAR) {
        return 8;
    } else if (data_type == MEDIUM_INT) {
        return 24;
    } else if (data_type == BOOLEAN) {
        return 1;
    } else {
        return -1;
    }
}

int freeElem(int ind) {
}

void gcRun() {
    // performs mark and sweep
}

void *gcThread(void *arg) {
    // not sure if this is how it should be
    while (1) {
        sleep(2);
        gcRun();
    }
}

void initScope() {
    var_stack->push(-1);
}

void endScope() {
}

int createMem(size_t bytes) {
    mem = (Memory *)malloc(sizeof(Memory));
    if (mem->init(bytes) == -1) {
        free(mem);
        return -1;
    }

    page_table = (PageTable *)malloc(sizeof(PageTable));
    page_table->init();

    var_stack = (Stack *)malloc(sizeof(Stack));
    var_stack->init();

    pthread_t gc_tid;
    pthread_create(&gc_tid, NULL, gcThread, NULL);
    return 0;
}

MyType createVar(DataType type) {
}

int assignVar(MyType var, int val) {
}

int assignVar(MyType var, char val) {
}

int assignVar(MyType var, bool val) {
}

MyType createArr(DataType type, int len) {
}

int assignArr(MyType arr, int val[]) {
}

int assignArr(MyType arr, char val[]) {
}

int assignArr(MyType arr, bool val[]) {
}

int assignArr(MyType arr, int index, int val) {
}

int assignArr(MyType arr, int index, char val) {
}

int assignArr(MyType arr, int index, bool val) {
}

int main() {
    
}