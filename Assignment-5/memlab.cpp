#include "memlab.h"

#include <pthread.h>
#include <signal.h>

#include <cstring>

using namespace std;

#define DEBUG(msg, ...) printf("\x1b[32m[DEBUG] " msg " \x1b[0m\n", ##__VA_ARGS__);
#define ERROR(msg, ...) printf("\x1b[31m[ERROR] " msg " \x1b[0m\n", ##__VA_ARGS__);
#define WARNING(msg, ...) printf("\x1b[31m[WARNING] " msg "\x1b[0m\n", ##__VA_ARGS__);
#define LIBRARY(msg, ...) printf("\x1b[32m[LIBRARY] " msg " \x1b[0m\n", ##__VA_ARGS__);
#define PAGE_TABLE(msg, ...) printf("\x1b[33m[PAGE TABLE] " msg " \x1b[0m\n", ##__VA_ARGS__);
#define WORD_ALIGN(msg, ...) printf("\x1b[34m[WORD ALIGNMENT] " msg " \x1b[0m\n", ##__VA_ARGS__);
#define GC(msg, ...) printf("\x1b[36m[GC] " msg "\x1b[0m\n", ##__VA_ARGS__);
#define STACK(msg, ...) printf("\x1b[37m[STACK] " msg "\x1b[0m\n", ##__VA_ARGS__);
#define MEMORY(msg, ...) printf("\x1b[35m[MEMORY] " msg "\x1b[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\x1b[37m[INFO] " msg "\x1b[0m\n", ##__VA_ARGS__);

typedef unsigned int u_int;
typedef long unsigned int u_long;

const size_t MAX_PT_ENTRIES = 1024;
const size_t MAX_STACK_SIZE = 1024;

const int MEDIUM_INT_MAX = (1 << 23) - 1;
const int MEDIUM_INT_MIN = -(1 << 23);

const int GC_SLEEP_MS = 50; 
const double COMPACTION_RATIO_THRESHOLD = 2;

#define LOCK(mutex_p)                                                            \
    do {                                                                         \
        int ret = pthread_mutex_lock(mutex_p);                                   \
        if (ret != 0) {                                                          \
            ERROR("%d: pthread_mutex_lock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

#define UNLOCK(mutex_p)                                                            \
    do {                                                                           \
        int ret = pthread_mutex_unlock(mutex_p);                                   \
        if (ret != 0) {                                                            \
            ERROR("%d: pthread_mutex_unlock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                               \
        }                                                                          \
    } while (0)

/*
Logs:
calling library functions
creating/updating page table entries
word alignment
steps of garbage collection

TODO:
add logs for word alignment
*/

struct Memory {
    int *start;
    int *end;
    size_t size;  // in words
    size_t totalFree;
    u_int numFreeBlocks;
    size_t currMaxFree;
    pthread_mutex_t mutex;

    int init(size_t bytes) {
        bytes = ((bytes + 3) >> 2) << 2;
        start = (int *)malloc(bytes);
        if (start == NULL) {
            return -1;
        }
        end = start + (bytes >> 2);
        size = bytes >> 2;  // in words
        *start = (bytes >> 2) << 1;
        *(start + (bytes >> 2) - 1) = (bytes >> 2) << 1;

        totalFree = bytes >> 2;
        numFreeBlocks = 1;
        currMaxFree = bytes >> 2;

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutex_init(&mutex, &attr);

        MEMORY("Memory segment created");
        MEMORY("start = %lu", (u_long)start);
        MEMORY("size = %lu (in words)", size);
        return 0;
    }

    int getOffset(int *p) {
        return (int)(p - start);
    }

    int *getAddr(int offset) {
        return (start + offset);
    }

    int *findFreeBlock(size_t sz) {  // sz is the size required for the data (in words)
        MEMORY("Finding free block for %lu word(s) of data", sz);
        int *p = start;
        while ((p < end) && ((*p & 1) || ((size_t)(*p >> 1) < sz + 2))) {
            p = p + (*p >> 1);
        }
        if (p < end) {
            MEMORY("Found free block at %lu", (u_long)p);
            return p;
        } else {
            MEMORY("No free block found");
            return NULL;
        }
    }

    void allocateBlock(int *p, size_t sz) {  // sz is the size required for the data (in words)
        sz += 2;
        u_int old_size = *p >> 1;       // mask out low bit
        *p = (sz << 1) | 1;             // set new length and allocated bit for header
        *(p + sz - 1) = (sz << 1) | 1;  // same for footer

        if (sz < old_size) {
            *(p + sz) = (old_size - sz) << 1;            // set length in remaining for header
            *(p + old_size - 1) = (old_size - sz) << 1;  // same for footer
        }

        totalFree -= sz;
        if (sz == old_size) {
            numFreeBlocks--;
        }
        if (old_size == currMaxFree) {
            currMaxFree -= sz;
        }
        currMaxFree = max(currMaxFree, totalFree / (numFreeBlocks + 1));
        MEMORY("Allocated block at %lu for %lu word(s) of data", (u_long)p, sz - 2);
    }

    void freeBlock(int *p) {
        MEMORY("Freeing block at %lu", (u_long)p);
        *p = *p & -2;  // clear allocated flag in header
        u_int curr_size = *p >> 1;
        *(p + curr_size - 1) = *(p + curr_size - 1) & -2;  // clear allocated flag in footer

        totalFree += curr_size;
        numFreeBlocks++;

        int *next = p + curr_size;                // find next block
        if ((next != end) && (*next & 1) == 0) {  // if next block is free
            MEMORY("Coalescing with next block at %lu", (u_long)next);
            u_int next_size = *next >> 1;
            *p = (curr_size + next_size) << 1;                                // merge with next block
            *(p + curr_size + next_size - 1) = (curr_size + next_size) << 1;  // set length in footer
            numFreeBlocks--;
            curr_size += next_size;
        }

        if ((p != start) && (*(p - 1) & 1) == 0) {  // if previous block is free
            u_int prev_size = *(p - 1) >> 1;
            MEMORY("Coalescing with previous block at %lu", (u_long)(p - prev_size));
            *(p - prev_size) = (prev_size + curr_size) << 1;      // set length in header of prev
            *(p + curr_size - 1) = (prev_size + curr_size) << 1;  // set length in footer
            numFreeBlocks--;
            curr_size += prev_size;
        }

        currMaxFree = max(currMaxFree, (size_t)curr_size);
        currMaxFree = max(currMaxFree, totalFree / (numFreeBlocks + 1));
    }
};

u_int counterToIdx(u_int p) {
    return (p >> 2);
}

u_int idxToCounter(u_int p) {
    return (p << 2);
}

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
        printf("%10d %6d %6d\n", addr, valid, marked);
    }
};

struct PageTable {
    PageTableEntry pt[MAX_PT_ENTRIES];
    u_int head, tail;
    size_t size;
    pthread_mutex_t mutex;

    void init() {
        for (size_t i = 0; i < MAX_PT_ENTRIES; i++) {
            pt[i].init();
            pt[i].addr = i + 1;
        }
        head = 0;
        tail = MAX_PT_ENTRIES - 1;
        size = 0;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutex_init(&mutex, &attr);
        PAGE_TABLE("Page table initialized");
    }

    int insert(u_int addr) {
        if (size == MAX_PT_ENTRIES) {
            PAGE_TABLE("Page table is full, insert failed");
            return -1;
        }
        u_int idx = head;
        u_int next = pt[head].addr;
        pt[idx].addr = addr;
        pt[idx].valid = 1;
        pt[idx].marked = 1;
        size++;
        if (size < MAX_PT_ENTRIES) {
            head = next;
        } else {
            head = MAX_PT_ENTRIES;
            tail = MAX_PT_ENTRIES;
        }
        PAGE_TABLE("Inserted new page table entry with memory offset %d at array index %d", addr, idx);
        return idx;
    }

    int remove(u_int idx) {
        if (size == 0 || !pt[idx].valid) {
            PAGE_TABLE("Page table is empty or entry index %d is invalid, remove failed", idx);
            return -1;
        }
        pt[idx].valid = 0;
        pt[tail].addr = idx;
        tail = idx;
        size--;
        if (size == MAX_PT_ENTRIES - 1) {
            head = tail;
        }
        PAGE_TABLE("Removed entry with array index %d in the page table", idx);
        return 0;
    }

    void print() {
        printf("\nPage Table:\n");
        printf("Head: %d, Tail: %d, Size: %lu\n", head, tail, size);
        printf("Index     Entry  Valid  Marked\n");
        for (size_t i = 0; i < MAX_PT_ENTRIES; i++) {
            printf("%3ld ", i);
            pt[i].print();
        }
    }
};

struct Stack {
    int st[MAX_STACK_SIZE];
    size_t size;

    void init() {
        size = 0;
        STACK("Stack initialized");
    }

    int top() {
        if (size == 0) {
            return -2;
        }
        return st[size - 1];
    }

    int push(int v) {
        if (size == MAX_STACK_SIZE) {
            STACK("Stack is full, push failed");
            return -2;
        }
        st[size++] = v;
        STACK("Pushed %d onto stack", v);
        return 0;
    }

    int pop() {
        if (size == 0) {
            STACK("Stack is empty, pop failed");
            return -2;
        }
        int v = st[--size];
        STACK("Popped %d from stack", v);
        return v;
    }

    void print() {
        printf("\nGlobal Variable Stack:\n");
        printf("Size: %lu\n", size);
        for (size_t i = 0; i < size; i++) {
            printf("%d ", st[i]);
        }
        printf("\n");
    }
};

Memory *mem;
PageTable *page_table;
Stack *var_stack;

void freeElem(u_int idx) {
    GC("freeElem called for array index %d in page table", idx);
    int ret = page_table->remove(idx);
    if (ret == -1) {
        throw runtime_error("freeElem: Invalid Index");
    }
    mem->freeBlock(mem->getAddr(page_table->pt[idx].addr));
}

void freeElem(MyType &var) {
    LIBRARY("freeElem called for variable with counter = %d", var.ind);
    LOCK(&page_table->mutex);
    LOCK(&mem->mutex);
    if (page_table->pt[counterToIdx(var.ind)].valid) {
        freeElem(counterToIdx(var.ind));
    }
    UNLOCK(&mem->mutex);
    UNLOCK(&page_table->mutex);
}

void calcNewOffsets() {
    int *p = mem->start;
    int offset = 0;
    while (p < mem->end) {
        if ((*p & 1) == 0) {
            offset += (*p >> 1);
        } else {
            *(p + (*p >> 1) - 1) = (((p - offset) - mem->start) << 1) | 1;
        }
        p = p + (*p >> 1);
    }
    GC("Completed calculating new offsets for blocks for compaction");
}

void updatePageTable() {
    for (size_t i = 0; i < MAX_PT_ENTRIES; i++) {
        if (page_table->pt[i].valid) {
            int *p = mem->getAddr(page_table->pt[i].addr);
            int newAddr = *(p + (*p >> 1) - 1) >> 1;
            PAGE_TABLE("Index: %ld, Old addr: %d, New addr: %d", i, page_table->pt[i].addr, newAddr);
            page_table->pt[i].addr = newAddr;
        }
    }
    GC("Page table updated for compaction");
}

void compactMemory() {
    GC("Starting memory compaction");
    ;
    calcNewOffsets();
    updatePageTable();
    int *p = mem->start;
    int *next = p + (*p >> 1);
    while (next != mem->end) {
        if ((*p & 1) == 0 && (*next & 1) == 1) {
            int curr_size = *p >> 1;
            int next_size = *next >> 1;
            memcpy(p, next, next_size << 2);
            p = p + next_size;
            *p = curr_size << 1;
            *(p + curr_size - 1) = curr_size << 1;
            next = p + curr_size;
            if (next != mem->end && (*next & 1) == 0) {
                curr_size = curr_size + (*next >> 1);
                *p = curr_size << 1;
                *(p + curr_size - 1) = curr_size << 1;
                next = p + curr_size;
            }
        } else {
            p = next;
            next = p + (*p >> 1);
        }
    }
    GC("Blocks rearranged after compaction");
    p = mem->start;
    while (p < mem->end) {
        *(p + (*p >> 1) - 1) = *p;
        p = p + (*p >> 1);
    }
    GC("Block footers updated");
    mem->numFreeBlocks = 1;
    mem->currMaxFree = mem->totalFree;
    GC("Memory compaction completed");
}

pthread_t gc_tid;

void gcRun() {
    GC("gcRun called");
    for (size_t i = 0; i < MAX_PT_ENTRIES; i++) {
        LOCK(&mem->mutex);
        LOCK(&page_table->mutex);
        if (page_table->pt[i].valid && !page_table->pt[i].marked) {
            freeElem(i);
        }
        UNLOCK(&page_table->mutex);
        UNLOCK(&mem->mutex);
    }

    double ratio = (double)mem->totalFree / (double)(mem->currMaxFree + 1);
    if (ratio >= COMPACTION_RATIO_THRESHOLD) {
        GC("Ratio = %f", ratio);
        LOCK(&mem->mutex);
        LOCK(&page_table->mutex);
        compactMemory();
        UNLOCK(&page_table->mutex);
        UNLOCK(&mem->mutex);
    }
}

void gcActivate() {
    GC("gcActivate called");
    pthread_kill(gc_tid, SIGUSR1);
}

void handleSIGUSR1(int sig) {
    GC("SIGUSR1 received");
    gcRun();
}

void *gcThread(void *arg) {
    signal(SIGUSR1, handleSIGUSR1);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    while (1) {
        usleep(GC_SLEEP_MS * 1000);  // adjust time
        pthread_sigmask(SIG_BLOCK, &mask, NULL);
        GC("Garbage collection thread awakened");
        gcRun();
        pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    }
}

void initScope() {
    LIBRARY("initScope called");
    if (var_stack->push(-1) < 0) {
        throw runtime_error("initScope: Stack full, cannot push");
    }
}

void endScope() {
    LIBRARY("endScope called");
    int ind;
    do {
        ind = var_stack->pop();
        if (ind == -2) {
            throw runtime_error("endScope: Stack empty, cannot pop");
        }
        if (ind >= 0) {
            LOCK(&page_table->mutex);
            page_table->pt[counterToIdx(ind)].marked = 0;
            UNLOCK(&page_table->mutex);
        }
    } while (ind >= 0);
}

void cleanExit() {
    LIBRARY("cleanExit called");
    LOCK(&mem->mutex);
    LOCK(&page_table->mutex);
    pthread_mutex_destroy(&mem->mutex);
    pthread_mutex_destroy(&page_table->mutex);
    free(mem->start);
    free(mem);
    free(page_table);
    free(var_stack);
    exit(0);
}

int getAddrForIdx(DataType t, int idx) {
    int _count = (1 << 2) / getSize(t);
    int _idx = idx / _count;
    return _idx;
}

int getOffsetForIdx(DataType t, int idx) {
    int _count = (1 << 2) / getSize(t);
    int _idx = idx / _count;
    return (idx - _idx * _count) * getSize(t);
}

void createMem(size_t bytes) {
    LIBRARY("createMem called");
    mem = (Memory *)malloc(sizeof(Memory));
    if (mem->init(bytes) == -1) {
        throw runtime_error("createMem: Memory allocation failed");
    }

    page_table = (PageTable *)malloc(sizeof(PageTable));
    page_table->init();

    var_stack = (Stack *)malloc(sizeof(Stack));
    var_stack->init();

    pthread_create(&gc_tid, NULL, gcThread, NULL);
}

MyType create(VarType var_type, DataType data_type, u_int len, u_int size_req) {
    LOCK(&mem->mutex);
    int *p = mem->findFreeBlock(size_req);
    if (p == NULL) {
        LOCK(&page_table->mutex);
        compactMemory();
        p = mem->findFreeBlock(size_req);
        UNLOCK(&page_table->mutex);
        if (p == NULL) {
            throw runtime_error("create: No free block in memory");
        }
    }
    mem->allocateBlock(p, size_req);
    int addr = mem->getOffset(p);
    LOCK(&page_table->mutex);
    int idx = page_table->insert(addr);
    if (idx < 0) {
        throw runtime_error("create: No free space in page table");
    }
    UNLOCK(&page_table->mutex);
    UNLOCK(&mem->mutex);
    u_int ind = idxToCounter(idx);
    if (var_stack->push(ind) < 0) {
        throw runtime_error("create: Stack full, cannot push");
    }
    return MyType(ind, var_type, data_type, len);
}

MyType createVar(DataType type) {
    LIBRARY("createVar called with type = %s", getDataTypeStr(type).c_str());
    return create(PRIMITIVE, type, 1, 1);
}

void validate(MyType &var, VarType type) {
    if (var.var_type != type) {
        string str = (type == PRIMITIVE ? "primitive" : "array");
        throw runtime_error("create: Variable is not a " + str);
    }
    if (!page_table->pt[counterToIdx(var.ind)].valid) {
        throw runtime_error("create: Variable is not valid");
    }
}

void assignVar(MyType &var, int val) {
    LIBRARY("assignVar (int) called for variable with counter = %d and value = %d", var.ind, val);
    validate(var, PRIMITIVE);
    if (var.data_type != INT && var.data_type != MEDIUM_INT) {
        throw runtime_error("assignVar (int): Type mismatch. Data type of variable is " + getDataTypeStr(var.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(var.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        if (var.data_type == INT) {
            memcpy(p, &val, 4);
        } else if (var.data_type == MEDIUM_INT) {
            if (val < MEDIUM_INT_MIN || val > MEDIUM_INT_MAX) {
                WARNING("assignVar (int): Variable is of type medium int, value is out of range, compressing");
            }
            memcpy(p, &val, 4);
        }
        UNLOCK(&mem->mutex);
    }
}

void assignVar(MyType &var, char val) {
    LIBRARY("assignVar (char) called for variable with counter = %d and value = %c", var.ind, val);
    validate(var, PRIMITIVE);
    if (var.data_type != CHAR) {
        throw runtime_error("assignVar (char): Type mismatch. Data type of variable is " + getDataTypeStr(var.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(var.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        int temp = (int)val;
        memcpy(p, &temp, 4);
        UNLOCK(&mem->mutex);
    }
}

void assignVar(MyType &var, bool val) {
    LIBRARY("assignVar (bool) called for variable with counter = %d and value = %d", var.ind, val);
    validate(var, PRIMITIVE);
    if (var.data_type != BOOLEAN) {
        throw runtime_error("assignVar (bool): Type mismatch. Data type of variable is " + getDataTypeStr(var.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(var.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        int temp = (int)val;
        memcpy(p, &temp, 4);
        UNLOCK(&mem->mutex);
    }
}

void readVar(MyType &var, void *ptr) {
    LIBRARY("readVar called for variable with counter = %d", var.ind);
    validate(var, PRIMITIVE);
    int size = getSize(var.data_type);
    LOCK(&mem->mutex);
    u_int idx = counterToIdx(var.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    int t = *(int *)p;
    if (var.data_type == MEDIUM_INT) {
        if (t & (1 << 23)) {
            t = t | 0xff000000;
        }
    }
    if (var.data_type == MEDIUM_INT) {
        size++;
    }
    memcpy(ptr, &t, size);
    UNLOCK(&mem->mutex);
}

MyType createArr(DataType type, int len) {
    LIBRARY("createArr called with type = %s and len = %d", getDataTypeStr(type).c_str(), len);
    if (len <= 0) {
        throw runtime_error("createArr: Length of array should be greater than 0");
    }
    u_int size_req;
    if (type == INT || type == MEDIUM_INT) {
        size_req = len;
    } else {
        size_req = (len + 3) >> 2;
    }
    return create(ARRAY, type, len, size_req);
}

void assignArr(MyType &arr, int val[]) {
    LIBRARY("assignArr (int) called for array with counter = %d", arr.ind);
    validate(arr, ARRAY);
    if (arr.data_type != INT && arr.data_type != MEDIUM_INT) {
        throw runtime_error("assignArr (int[]): Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(arr.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        if (arr.data_type == INT) {
            for (size_t i = 0; i < arr.len; i++) {
                memcpy(p + i, &val[i], 4);
            }
        } else if (arr.data_type == MEDIUM_INT) {
            for (size_t i = 0; i < arr.len; i++) {
                if (val[i] < MEDIUM_INT_MIN || val[i] > MEDIUM_INT_MAX) {
                    WARNING("Variable at index %zu is of type medium int, value is out of range, compressing", i);
                }
                memcpy(p + i, &val[i], 4);
            }
        }
        UNLOCK(&mem->mutex);
    }
}

void assignArr(MyType &arr, char val[]) {
    LIBRARY("assignArr (char) called for array with counter = %d", arr.ind);
    validate(arr, ARRAY);
    if (arr.data_type != CHAR) {
        throw runtime_error("assignArr (char[]): Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(arr.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        for (size_t i = 0; i < arr.len; i += 4) {
            u_int temp = 0;
            for (size_t j = 0; j < 4; j++) {
                char c = (i + j < arr.len) ? val[i + j] : 0;
                temp = temp | ((u_int)c << (j * 8));
            }
            memcpy((char *)p + i, &temp, 4);
        }
        UNLOCK(&mem->mutex);
    }
}

void assignArr(MyType &arr, bool val[]) {
    LIBRARY("assignArr (bool) called for array with counter = %d", arr.ind);
    validate(arr, ARRAY);
    if (arr.data_type != BOOLEAN) {
        throw runtime_error("assignArr (bool[]): Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(arr.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        for (size_t i = 0; i < arr.len; i += 4) {
            u_int temp = 0;
            for (size_t j = 0; j < 4; j++) {
                bool b = (i + j < arr.len) ? val[i + j] : 0;
                temp = temp | ((u_int)b << (j * 8));
            }
            memcpy((char *)p + i, &temp, 4);
        }
        UNLOCK(&mem->mutex);
    }
}

void assignArr(MyType &arr, int index, int val) {
    LIBRARY("assignArr (int) called for array with counter = %d at index = %d and value = %d", arr.ind, index, val);
    validate(arr, ARRAY);
    if (index < 0 || index >= (int)arr.len) {
        throw runtime_error("assignArr (index, int): Index out of range");
    }
    if (arr.data_type != INT && arr.data_type != MEDIUM_INT) {
        throw runtime_error("assignArr (index, int): Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(arr.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        if (arr.data_type == INT) {
            memcpy(p + index, &val, 4);
        } else if (arr.data_type == MEDIUM_INT) {
            if (val < MEDIUM_INT_MIN || val > MEDIUM_INT_MAX) {
                WARNING("assignArr (index, int): Variable at index %d is of type medium int, value is out of range, compressing", index);
            }
            memcpy(p + index, &val, 4);
        }
        UNLOCK(&mem->mutex);
    }
}

void assignArr(MyType &arr, int index, char val) {
    LIBRARY("assignArr (char) called for array with counter = %d at index = %d and value = %c", arr.ind, index, val);
    validate(arr, ARRAY);
    if (index < 0 || index >= (int)arr.len) {
        throw runtime_error("assignArr (index, char): Index out of range");
    }
    if (arr.data_type != CHAR) {
        throw runtime_error("assignArr (index, char): Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(arr.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        int *q = p + getAddrForIdx(arr.data_type, index);
        int offset = getOffsetForIdx(arr.data_type, index);
        int temp = *q;
        temp = temp & ~(0xff << (offset * 8));
        temp = temp | (val << (offset * 8));
        memcpy(q, &temp, 4);
        // memcpy((char *)p + index, &val, getSize(CHAR));
        UNLOCK(&mem->mutex);
    }
}

void assignArr(MyType &arr, int index, bool val) {
    LIBRARY("assignArr (bool) called for array with counter = %d at index = %d and value = %d", arr.ind, index, val);
    validate(arr, ARRAY);
    if (index < 0 || index >= (int)arr.len) {
        throw runtime_error("assignArr (index, bool): Index out of range");
    }
    if (arr.data_type != BOOLEAN) {
        throw runtime_error("assignArr (index, bool): Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    } else {
        LOCK(&mem->mutex);
        u_int idx = counterToIdx(arr.ind);
        int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
        int *q = p + getAddrForIdx(arr.data_type, index);
        int offset = getOffsetForIdx(arr.data_type, index);
        int temp = *q;
        temp = temp & ~(0xff << (offset * 8));
        temp = temp | (val << (offset * 8));
        memcpy(q, &temp, 4);
        // memcpy((char *)p + index, &val, getSize(BOOLEAN));
        UNLOCK(&mem->mutex);
    }
}

// TODO: add word alignment
void readArr(MyType &arr, void *ptr) {
    LIBRARY("readArr called for array with counter = %d", arr.ind);
    validate(arr, ARRAY);
    int size = getSize(arr.data_type);
    LOCK(&mem->mutex);
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == INT) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy((int *)ptr + i, p + i, size);
        }
    } else if (arr.data_type == MEDIUM_INT) {
        for (size_t i = 0; i < arr.len; i++) {
            int temp = *(p + i);
            if (temp & (1 << 23)) {
                temp = temp | 0xff000000;
            }
            memcpy((int *)ptr + i, &temp, size);
        }
    } else {
        size_t blocks = (arr.len >> 2);
        for (size_t i = 0; i < blocks; i++) {
            memcpy((int *)ptr + i, p + i, 4);
        }
        size_t rem = arr.len - (blocks << 2);
        if (rem > 0) {
            memcpy((int *)ptr + blocks, p + blocks, rem);
        }
    }

    // for (size_t i = 0; i < arr.len; i++) {
    //     if (arr.data_type == INT) {
    //         memcpy((int *)ptr + i, p + i, size);
    //     } else if (arr.data_type == MEDIUM_INT) {
    //         int num_val = *(int *)(p + i);
    //         if (num_val & (1 << 23)) {
    //             num_val = num_val | 0xff000000;
    //         }
    //         memcpy((int *)ptr + i, &num_val, 4);
    //     } else if (arr.data_type == CHAR) {
    //         memcpy((char *)ptr + i, (char *)p + i, size);
    //     } else if (arr.data_type == BOOLEAN) {
    //         memcpy((bool *)ptr + i, (char *)p + i, size);
    //     }
    // }
    UNLOCK(&mem->mutex);
}

void readArr(MyType &arr, int index, void *ptr) {
    LIBRARY("readArr called for array with counter = %d at index = %d", arr.ind, index);
    validate(arr, ARRAY);
    if (index < 0 || index >= (int)arr.len) {
        throw runtime_error("readArr (index): Index out of range");
    }
    int size = getSize(arr.data_type);
    LOCK(&mem->mutex);
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    p = p + getAddrForIdx(arr.data_type, index);
    int offset = getOffsetForIdx(arr.data_type, index);
    // DEBUG("p = %lu, index = %d, offset = %d", (u_long)p, index, offset);
    int t = *p;
    if (arr.data_type == MEDIUM_INT) {
        if (t & (1 << 23)) {
            t = t | 0xff000000;
        }
    }
    if (arr.data_type == MEDIUM_INT) {
        size++;
    }
    memcpy(ptr, (char *)(&t) + offset, size);
    UNLOCK(&mem->mutex);
}

// void testCode() {
//     createMem(1024 * 1024 * 512);  // 512 MB
//     initScope();
//     MyType p1 = createVar(INT);
//     // cout << "p1.addr: " << p1.ind << endl;
//     MyType p2 = createVar(BOOLEAN);
//     MyType p3 = createVar(MEDIUM_INT);
//     MyType p4 = createVar(CHAR);
//     usleep(150 * 1000);
//     freeElem(p1);
//     freeElem(p3);
//     MyType p5 = createVar(INT);
//     // cout << "p5.addr: " << p5.ind << endl;
//     endScope();
//     initScope();
//     usleep(100 * 1000);
//     MyType q1 = createVar(INT);
//     MyType q2 = createVar(BOOLEAN);
//     MyType q3 = createVar(MEDIUM_INT);
//     MyType q4 = createVar(CHAR);
//     endScope();
//     usleep(10000);
//     cleanExit();
// }

// void testCompaction() {
//     createMem(144);
//     MyType p1 = createVar(INT);
//     MyType p2 = createVar(INT);
//     MyType arr1 = createArr(INT, 10);
//     MyType p3 = createVar(INT);
//     MyType p4 = createVar(INT);
//     MyType arr2 = createArr(INT, 10);
//     // usleep(1000);
//     freeElem(p1);
//     freeElem(p2);
//     // freeElem(arr1);
//     freeElem(p4);
//     compactMemory();
//     // int* arrptr1 = symTable->getPtr(arr1.addr >> 2);
//     // cout << "arrptr1: " << arrptr1 - mem->start << endl;
//     // int* ptr3 = symTable->getPtr(p3.addr >> 2);
//     // cout << "ptr3:" << ptr3 - mem->start << endl;
//     // int* arrptr2 = symTable->getPtr(arr2.addr >> 2);
//     // cout << "arrptr2: " << arrptr2 - mem->start << endl;
//     // cout << endl;
//     int* p = mem->start;
//     cout << (*(p) >> 1) << " " << (*(p)&1) << endl;
//     cout << (*(p + 11) >> 1) << " " << (*(p + 11) & 1) << endl;
//     cout << endl;
//     cout << (*(p + 12) >> 1) << " " << (*(p + 12) & 1) << endl;
//     cout << (*(p + 14) >> 1) << " " << (*(p + 14) & 1) << endl;
//     cout << endl;
//     cout << (*(p + 15) >> 1) << " " << (*(p + 15) & 1) << endl;
//     cout << (*(p + 26) >> 1) << " " << (*(p + 26) & 1) << endl;
//     cout << endl;
//     cout << (*(p + 27) >> 1) << " " << (*(p + 27) & 1) << endl;
//     p = mem->end - 1;
//     cout << (*(p) >> 1) << " " << (*(p)&1) << endl;

//     cleanExit();
// }

void testCompactionCall() {
    createMem(144);
    MyType p1 = createVar(INT);
    assignVar(p1, 5);
    MyType p2 = createVar(INT);
    assignVar(p2, 10);
    MyType arr1 = createArr(INT, 8);
    int temp[] = {1, 2, 3, 4, 5, 6, 7, 8};
    assignArr(arr1, temp);
    MyType p3 = createVar(INT);
    assignVar(p3, -15);
    MyType p4 = createVar(INT);
    assignVar(p4, -10);
    MyType arr2 = createArr(INT, 6);
    int temp1[] = {1, 2, 3, 4, 5, 6};
    assignArr(arr2, temp1);
    // freeElem(arr1);
    freeElem(p1);
    freeElem(p2);
    freeElem(p3);
    printf("Total free memory: %lu\n", mem->totalFree);
    printf("Total free blocks: %d\n", mem->numFreeBlocks);
    printf("Biggest free block: %lu\n", mem->currMaxFree);
    MyType arr3 = createArr(INT, 6);
    int temp2[] = {10, 20, 30, 40, 50, 60};
    assignArr(arr3, temp2);
    // usleep(60 * 1000);
    // gcRun();
    // compactMemory();
    // int* ptr1 = symTable->getPtr(p1.addr >> 2);
    // cout << "ptr1:" << ptr1 - mem->start << endl;
    // int* ptr2 = symTable->getPtr(p2.addr >> 2);
    // cout << "ptr2:" << ptr2 - mem->start << endl;
    // int* ptr3 = symTable->getPtr(p3.addr >> 2);
    // cout << "ptr3:" << ptr3 - mem->start << endl;
    // int* ptr4 = symTable->getPtr(p4.addr >> 2);
    // cout << "ptr4:" << ptr4 - mem->start << endl;
    // int* arrptr2 = symTable->getPtr(arr2.addr >> 2);
    // cout << "arrptr2: " << arrptr2 - mem->start << endl;
    // int* arrptr3 = symTable->getPtr(arr3.addr >> 2);
    // cout << "arrptr3: " << arrptr3 - mem->start << endl;
    printf("Total free memory: %lu\n", mem->totalFree);
    printf("Total free blocks: %d\n", mem->numFreeBlocks);
    printf("Biggest free block: %lu\n", mem->currMaxFree);

    int x;
    // readVar(p1, &x);
    // printf("p1: %d\n", x);
    // readVar(p2, &x);
    // printf("p2: %d\n", x);
    // readVar(p3, &x);
    // printf("p3: %d\n", x);
    readVar(p4, &x);
    printf("p4: %d\n", x);
    int t[6];
    readArr(arr2, t);
    for (int i = 0; i < 6; i++) {
        printf("arr2[%d]: %d\n", i, t[i]);
    }
    readArr(arr3, t);
    for (int i = 0; i < 6; i++) {
        printf("arr3[%d]: %d\n", i, t[i]);
    }
}

// int main() {
//     createMem(120);

//     // testCode();
//     // testCompactionCall();

//     // MyType aa = createVar(INT);
//     // aa.print();
//     // assignVar(aa, -19);
//     // int x;
//     // readVar(aa, &x);
//     // DEBUG("value of aa = %d", x);

//     // int *p = mem->start;
//     // printf("%d, %d\n", *p >> 1, *p & 1);
//     // printf("%d, %d\n", *(p + 2) >> 1, *(p + 2) & 1);
//     // printf("%d, %d\n", *(p + 3) >> 1, *(p + 3) & 1);

//     // MyType a = createVar(CHAR);
//     // a.print();
//     // page_table->pt[counterToIdx(a.ind)].print();
//     // assignVar(a, 'b');
//     // char v;
//     // readVar(a, &v);
//     // DEBUG("value of a = %c", v);

//     // MyType b = createVar(BOOLEAN);
//     // b.print();
//     // page_table->pt[counterToIdx(b.ind)].print();
//     // assignVar(b, false);
//     // bool xx;
//     // readVar(b, &xx);
//     // // cout << xx << endl;
//     // DEBUG("value of b = %d", xx);

//     // MyType bb = createVar(MEDIUM_INT);
//     // bb.print();
//     // assignVar(bb, -19);
//     // int y;
//     // readVar(bb, &y);
//     // DEBUG("value of bb = %d", y);

//     // MyType arr1 = createArr(INT, 5);
//     // arr1.print();
//     // int arr1_val[] = {1, 2, -3, 4, -5};
//     // assignArr(arr1, arr1_val);
//     // int arr1_val_read[5];
//     // readArr(arr1, arr1_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr1_val_read[i]);
//     // }
//     // assignArr(arr1, 0, -6);
//     // assignArr(arr1, 1, -7);
//     // assignArr(arr1, 2, -8);
//     // assignArr(arr1, 3, -9);
//     // assignArr(arr1, 4, 25);
//     // readArr(arr1, arr1_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr1_val_read[i]);
//     // }
//     // int v;
//     // readArr(arr1, 0, &v);
//     // DEBUG("%d", v);
//     // readArr(arr1, 1, &v);
//     // DEBUG("%d", v);
//     // readArr(arr1, 2, &v);
//     // DEBUG("%d", v);
//     // readArr(arr1, 3, &v);
//     // DEBUG("%d", v);
//     // readArr(arr1, 4, &v);
//     // DEBUG("%d", v);

//     // MyType arr = createArr(MEDIUM_INT, 5);
//     // arr.print();
//     // int arr_val[] = {1, 2, -3, 4, -5};
//     // assignArr(arr, arr_val);
//     // int arr_val_read[5];
//     // readArr(arr, arr_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     // }
//     // printf("\n");
//     // assignArr(arr, 0, -6);
//     // assignArr(arr, 1, 7);
//     // assignArr(arr, 2, -8);
//     // assignArr(arr, 3, 9);
//     // assignArr(arr, 4, 25);
//     // readArr(arr, arr_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     // }
//     // int v;
//     // readArr(arr, 0, &v);
//     // DEBUG("%d", v);
//     // readArr(arr, 1, &v);
//     // DEBUG("%d", v);
//     // readArr(arr, 2, &v);
//     // DEBUG("%d", v);
//     // readArr(arr, 3, &v);
//     // DEBUG("%d", v);
//     // readArr(arr, 4, &v);
//     // DEBUG("%d", v);

//     // MyType arr2 = createArr(INT, 5);
//     // arr2.print();
//     // int arr2_val[] = {1, 2, -3, 4, -5};
//     // assignArr(arr2, arr2_val);
//     // int arr2_val_read[5];
//     // readArr(arr2, arr2_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr2_val_read[i]);
//     // }
//     //     assignArr(arr2, 0, -6);
//     //     assignArr(arr2, 1, -7);
//     //     assignArr(arr2, 2, -8);
//     //     assignArr(arr2, 3, -9);
//     //     assignArr(arr2, 4, 25);
//     //     readArr(arr2, arr2_val_read);
//     //     // for (size_t i = 0; i < 5; i++) {
//     //     //     DEBUG("arr_val_read[%zu] = %d", i, arr2_val_read[i]);
//     //     // }

//     //     freeElem(arr);

//     // MyType arr3 = createArr(CHAR, 5);
//     // char arr3_val[] = {'a', 'b', 'c', 'd', 'e'};
//     // assignArr(arr3, arr3_val);
//     // char arr3_val_read[5];
//     // readArr(arr3, arr3_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr3_val_read[%zu] = %c", i, arr3_val_read[i]);
//     // }
//     // printf("\n");
//     // assignArr(arr3, 0, 'f');
//     // assignArr(arr3, 1, 'g');
//     // assignArr(arr3, 2, 'h');
//     // assignArr(arr3, 3, 'i');
//     // assignArr(arr3, 4, 'j');
//     // // readArr(arr3, arr3_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     char cc;
//     //     readArr(arr3, i, &cc);
//     //     DEBUG("arr3_val_read[%zu] = %c", i, cc);
//     // }

//     MyType arr = createArr(BOOLEAN, 5);
//     arr.print();
//     bool arr_val[] = {true, false, true, false, true};
//     assignArr(arr, arr_val);
//     bool arr_val_read[5];
//     readArr(arr, arr_val_read);
//     for (size_t i = 0; i < 5; i++) {
//         DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     }
//     // printf("\n");
//     // assignArr(arr, 0, false);
//     // assignArr(arr, 1, true);
//     // assignArr(arr, 2, false);
//     // assignArr(arr, 3, true);
//     // assignArr(arr, 4, false);
//     // readArr(arr, arr_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     // }

//     // cleanExit();
// }