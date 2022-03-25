#include "memlab.h"

#include <pthread.h>

#include <csignal>
#include <cstring>
using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[SUCCESS] " msg " \033[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\033[1;32m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf("\033[1;32m[DEBUG] " msg " \033[0m\n", ##__VA_ARGS__);
#define TYPE_CHECK(msg, ...) printf("\033[1;33m[TYPE CHECK] " msg "\033[0m\n", ##__VA_ARGS__);

typedef unsigned int u_int;

const size_t MAX_PT_ENTRIES = 1024;
const size_t MAX_STACK_SIZE = 1024;

const int MEDIUM_INT_MAX = (1 << 23) - 1;
const int MEDIUM_INT_MIN = -(1 << 23);

void signal_handler(int sig) {
    if (sig == SIGINT) {
        cleanExit();
    } else if (sig == SIGSEGV) {
        ERROR("Segmentation fault");
        cleanExit();
    }
}

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

struct Memory {
    int *start;
    int *end;
    size_t size;  // in words
    pthread_mutex_t mutex;

    int init(size_t bytes) {
        bytes = ((bytes + 3) >> 2) << 2;
        start = (int *)malloc(bytes);
        if (start == NULL) {
            // ERROR("malloc failed");
            return -1;
        }
        DEBUG("start = %lu", start);
        end = start + (bytes >> 2);
        size = bytes >> 2;  // in words

        // set up initial free list
        *start = (bytes >> 2) << 1;
        *(start + (bytes >> 2) - 1) = (bytes >> 2) << 1;

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutex_init(&mutex, &attr);

        return 0;
    }

    int getOffset(int *p) {
        return (int)(p - start);
    }

    int *getAddr(int offset) {
        return (start + offset);
    }

    int *findFreeBlock(size_t sz) {  // sz is the size required for the data (in words)
        int *p = start;
        while ((p < end) && ((*p & 1) || ((size_t)(*p >> 1) < sz + 2))) {
            p = p + (*p >> 1);
        }
        DEBUG("findFreeBlock: %lu", p);
        if (p < end) {
            return p;
        } else {
            return NULL;
        }
    }

    void allocateBlock(int *p, size_t sz) {  // sz is the size required for the data (in words)
        sz += 2;
        u_int old_size = *p >> 1;  // mask out low bit
        // DEBUG("old_size = %d", old_size);
        *p = (sz << 1) | 1;  // set new length and allocated bit for header
        // DEBUG("new_size = %d", *p >> 1);
        *(p + sz - 1) = (sz << 1) | 1;  // same for footer

        if (sz < old_size) {
            *(p + sz) = (old_size - sz) << 1;            // set length in remaining for header
            *(p + old_size - 1) = (old_size - sz) << 1;  // same for footer
        }
    }

    void freeBlock(int *p) {
        *p = *p & -2;  // clear allocated flag in header
        u_int curr_size = *p >> 1;
        *(p + curr_size - 1) = *(p + curr_size - 1) & -2;  // clear allocated flag in footer

        int *next = p + curr_size;                // find next block
        if ((next != end) && (*next & 1) == 0) {  // if next block is free
            u_int next_size = *next >> 1;
            *p = (curr_size + next_size) << 1;                                // merge with next block
            *(p + curr_size + next_size - 1) = (curr_size + next_size) << 1;  // set length in footer
            curr_size += next_size;
        }

        if ((p != start) && (*(p - 1) & 1) == 0) {  // if previous block is free
            u_int prev_size = *(p - 1) >> 1;
            // *(p - 1) = (prev_size + curr_size) << 1;
            *(p - prev_size) = (prev_size + curr_size) << 1;      // set length in header of prev
            *(p + curr_size - 1) = (prev_size + curr_size) << 1;  // set length in footer
        }
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

Memory *mem;

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
            pt[i].addr = i + 1;
        }
        head = 0;
        tail = MAX_PT_ENTRIES - 1;
        size = 0;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutex_init(&mutex, &attr);
    }

    int insert(u_int addr) {
        if (size == MAX_PT_ENTRIES) {
            return -1;
        }
        u_int idx = head;
        u_int next = pt[head].addr;
        pt[idx].addr = addr;
        pt[idx].valid = 1;
        pt[idx].marked = 1;
        // head = next;
        size++;
        if (size < MAX_PT_ENTRIES) {
            head = next;
        } else {
            head = MAX_PT_ENTRIES;
            tail = MAX_PT_ENTRIES;
        }
        // print();
        return idx;
    }

    int remove(u_int idx) {
        if (size == 0 || !pt[idx].valid) {
            return -1;
        }
        pt[idx].valid = 0;
        pt[tail].addr = idx;
        tail = idx;
        size--;
        if (size == MAX_PT_ENTRIES - 1) {
            head = tail;
        }
        // print();
        return 0;
    }

    bool isValidEntry(u_int idx) {
        int *p = mem->getAddr(pt[idx].addr);
        return (pt[idx].valid && (*p & 1));
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
    }

    int top() {
        if (size == 0) {
            // ERROR("Stack empty");
            return -2;
        }
        return st[size - 1];
    }

    void push(int v) {
        if (size == MAX_STACK_SIZE) {
            // ERROR("Stack full, cannot push");
            return;
        }
        st[size++] = v;
    }

    int pop() {
        if (size == 0) {
            // ERROR("Stack empty, cannot pop");
            return -2;
        }
        return st[--size];
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

// global variables
PageTable *page_table;
Stack *var_stack;

int freeElem(u_int idx) {
    int ret = page_table->remove(idx);
    if (ret == -1) {
        return -1;
    }
    mem->freeBlock(mem->getAddr(page_table->pt[idx].addr));
    return 0;
}

int freeElem(MyType var) {
    int ret;
    LOCK(&page_table->mutex);
    LOCK(&mem->mutex);
    if (page_table->isValidEntry(counterToIdx(var.ind))) {
        ret = freeElem(counterToIdx(var.ind));
    }
    UNLOCK(&mem->mutex);
    UNLOCK(&page_table->mutex);
    return ret;
}

void calcNewOffsets() {
    int *p = mem->start;
    int offset = 0;
    while (p < mem->end) {
        if ((*p & 1) == 0) {
            offset += (*p >> 1);
        } else {
            *(p + (*p >> 1) - 1) = (((p - offset) - mem->start) << 1) | 1;
            // cout << "offset for: " << p - mem->start << " is " << (*(p + (*p >> 1) - 1) >> 1) << endl;
            // cout << "offsetval: " << offset << endl;
        }
        p = p + (*p >> 1);
    }
}

void updatePageTable() {
    for (int i = 0; i < MAX_PT_ENTRIES; i++) {
        if (page_table->isValidEntry(i)) {
            int *p = mem->getAddr(page_table->pt[i].addr);
            int newAddr = *(p + (*p >> 1) - 1) >> 1;
            // cout << "i, newWordId: " << i << " " << newWordId << endl;
            page_table->pt[i].addr = newAddr;
        }
    }
}

void compactMem() {
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

    p = mem->start;
    while (p < mem->end) {
        *(p + (*p >> 1) - 1) = *p;
        p = p + (*p >> 1);
    }
}

void gcRun() {
    // scan the page table if valid and unmarked, free that
    for (size_t i = 0; i < MAX_PT_ENTRIES; i++) {
        LOCK(&page_table->mutex);
        if (page_table->isValidEntry(i) && !page_table->pt[i].marked) {
            LOCK(&mem->mutex);
            freeElem(i);
            UNLOCK(&mem->mutex);
        }
        UNLOCK(&page_table->mutex);
    }
}

void *gcThread(void *arg) {
    // not sure if this is how it should be
    while (1) {
        usleep(50 * 1000);  // adjust time
        gcRun();
    }
}

void initScope() {
    var_stack->push(-1);
}

void endScope() {
    // keep popping till -1 is encountered
    int ind;
    do {
        ind = var_stack->pop();
        if (ind == -2) {
            throw runtime_error("Stack empty, cannot pop");
        }
        if (ind >= 0) {
            LOCK(&page_table->mutex);
            page_table->pt[counterToIdx(ind)].marked = 0;
            UNLOCK(&page_table->mutex);
        }
    } while (ind >= 0);
}

void cleanExit() {
    pthread_mutex_destroy(&mem->mutex);
    pthread_mutex_destroy(&page_table->mutex);
    free(mem->start);
    free(mem);
    free(page_table);
    free(var_stack);
    exit(0);
}

int createMem(size_t bytes) {
    signal(SIGINT, signal_handler);
    signal(SIGSEGV, signal_handler);
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

MyType create(VarType var_type, DataType data_type, u_int len, u_int size_req) {
    // find free block
    LOCK(&mem->mutex);
    int *p = mem->findFreeBlock(size_req);
    if (p == NULL) {
        UNLOCK(&mem->mutex);
        ERROR("No free block in memory");
        return MyType(-1, var_type, data_type, -1);
    }
    mem->allocateBlock(p, size_req);
    UNLOCK(&mem->mutex);

    // insert in page table
    int addr = mem->getOffset(p);
    LOCK(&page_table->mutex);
    int idx = page_table->insert(addr);
    DEBUG("Inserted in page table at index %d", idx);
    UNLOCK(&page_table->mutex);
    if (idx < 0) {
        ERROR("No free space in page table");
        return MyType(-1, var_type, data_type, -1);
    }
    u_int ind = idxToCounter(idx);
    // push to stack
    var_stack->push(ind);

    return MyType(ind, var_type, data_type, len);
}

MyType createVar(DataType type) {
    return create(PRIMITIVE, type, 1, 1);
}

int varChecks(MyType var) {
    if (var.var_type != PRIMITIVE) {
        TYPE_CHECK("Variable should be a primitive type");
        return -1;
    }
    if (!page_table->isValidEntry(counterToIdx(var.ind))) {
        ERROR("Variable is not valid");
        return -1;
    }
    return 0;
}

int assignVar(MyType var, int val) {
    if (varChecks(var) < 0) {
        return -1;
    }
    u_int idx = counterToIdx(var.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (var.data_type == INT) {
        memcpy(p, &val, getSize(INT));
        return 0;
    } else if (var.data_type == MEDIUM_INT) {
        if (val < MEDIUM_INT_MIN || val > MEDIUM_INT_MAX) {
            TYPE_CHECK("Variable is of type medium int, value is out of range, compressing");
        }
        memcpy(p, &val, getSize(MEDIUM_INT));
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(var.data_type).c_str());
        return -1;
    }
}

int assignVar(MyType var, char val) {
    if (varChecks(var) < 0) {
        return -1;
    }
    u_int idx = counterToIdx(var.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (var.data_type == CHAR) {
        memcpy(p, &val, getSize(CHAR));
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(var.data_type).c_str());
        return -1;
    }
}

int assignVar(MyType var, bool val) {
    if (varChecks(var) < 0) {
        return -1;
    }
    u_int idx = counterToIdx(var.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (var.data_type == BOOLEAN) {
        memcpy(p, &val, getSize(BOOLEAN));
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(var.data_type).c_str());
        return -1;
    }
}

int readVar(MyType var, void *ptr) {
    if (varChecks(var) < 0) {
        return -1;
    }
    int size = getSize(var.data_type);
    u_int idx = counterToIdx(var.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    memcpy(ptr, p, size);
    if (var.data_type == MEDIUM_INT) {
        int bit = (*((char *)p + size - 1) >> 7) & 1;
        // DEBUG("bit: %d", bit);
        memset((char *)ptr + size, -bit, 1);
    }
    return 0;
}

MyType createArr(DataType type, int len) {
    if (len <= 0) {
        ERROR("Length of array should be greater than 0");
        return MyType(-1, ARRAY, type, -1);
    }
    u_int size_req;
    if (type == INT || type == MEDIUM_INT) {
        size_req = len;
    } else {
        size_req = (len + 3) >> 2;
    }
    return create(ARRAY, type, len, size_req);
}

int arrChecks(MyType arr) {
    if (arr.var_type != ARRAY) {
        TYPE_CHECK("Variable should be an array type");
        return -1;
    }
    if (!page_table->isValidEntry(counterToIdx(arr.ind))) {
        ERROR("Array variable is not valid");
        return -1;
    }
    return 0;
}

int assignArr(MyType arr, int val[]) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == INT) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy(p + i, &val[i], getSize(INT));
        }
        return 0;
    } else if (arr.data_type == MEDIUM_INT) {
        for (size_t i = 0; i < arr.len; i++) {
            if (val[i] < MEDIUM_INT_MIN || val[i] > MEDIUM_INT_MAX) {
                TYPE_CHECK("Variable at index %zu is of type medium int, value is out of range, compressing", i);
            }
            memcpy(p + i, &val[i], getSize(MEDIUM_INT));
        }
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(arr.data_type).c_str());
        return -1;
    }
}

int assignArr(MyType arr, char val[]) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == CHAR) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy((char *)p + i, &val[i], getSize(CHAR));
        }
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(arr.data_type).c_str());
        return -1;
    }
}

int assignArr(MyType arr, bool val[]) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == BOOLEAN) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy((char *)p + i, &val[i], getSize(BOOLEAN));
        }
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(arr.data_type).c_str());
        return -1;
    }
}

int assignArr(MyType arr, int index, int val) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    if (index < 0 || index >= arr.len) {
        ERROR("Index out of range");
        return -1;
    }
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == INT) {
        memcpy(p + index, &val, getSize(INT));
        return 0;
    } else if (arr.data_type == MEDIUM_INT) {
        if (val < MEDIUM_INT_MIN || val > MEDIUM_INT_MAX) {
            TYPE_CHECK("Variable at index %d is of type medium int, value is out of range, compressing", index);
        }
        // DEBUG("p + index: %lu, index: %d, val: %d", p + index, index, val);
        memcpy(p + index, &val, getSize(MEDIUM_INT));
        // DEBUG("%d", *(p + index));
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(arr.data_type).c_str());
        return -1;
    }
}

int assignArr(MyType arr, int index, char val) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    if (index < 0 || index >= arr.len) {
        ERROR("Index out of range");
        return -1;
    }
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == CHAR) {
        memcpy((char *)p + index, &val, getSize(CHAR));
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(arr.data_type).c_str());
        return -1;
    }
}

int assignArr(MyType arr, int index, bool val) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    if (index < 0 || index >= arr.len) {
        ERROR("Index out of range");
        return -1;
    }
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == BOOLEAN) {
        memcpy((char *)p + index, &val, getSize(BOOLEAN));
        return 0;
    } else {
        TYPE_CHECK("Type mismatch. Data type of variable is %s", getDataTypeStr(arr.data_type).c_str());
        return -1;
    }
}

int readArr(MyType arr, void *ptr) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    int size = getSize(arr.data_type);
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    for (size_t i = 0; i < arr.len; i++) {
        if (arr.data_type == INT) {
            memcpy((int *)ptr + i, p + i, size);
        } else if (arr.data_type == MEDIUM_INT) {
            memcpy((int *)ptr + i, p + i, size);
            // DEBUG("addr =  %lu, val = %d", p + i, *(p + i));
            int bit = (*((char *)(p + i) + size - 1) >> 7) & 1;
            // DEBUG("bit: %d", bit);
            memset((char *)ptr + (i << 2) + size, -bit, 1);
        } else if (arr.data_type == CHAR) {
            memcpy((char *)ptr + i, (char *)p + i, size);
        } else if (arr.data_type == BOOLEAN) {
            memcpy((bool *)ptr + i, (char *)p + i, size);
        }
    }
    return 0;
}

int readArr(MyType arr, int index, void *ptr) {
    if (arrChecks(arr) < 0) {
        return -1;
    }
    if (index < 0 || index >= arr.len) {
        ERROR("Index out of range");
        return -1;
    }
    int size = getSize(arr.data_type);
    u_int idx = counterToIdx(arr.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    if (arr.data_type == INT || arr.data_type == MEDIUM_INT) {
        memcpy(ptr, p + index, size);
        if (arr.data_type == MEDIUM_INT) {
            int bit = (*((char *)(p + index) + size - 1) >> 7) & 1;
            memset((char *)ptr + size, -bit, 1);
        }
    } else {
        memcpy(ptr, (char *)p + index, size);
    }
}

// void testCode() {
//     createMem(1024 * 1024 * 512);  // 512 MB
//     initScope();
//     MyType p1 = createVar(INT);
//     cout << "p1.addr: " << p1.addr << endl;
//     MyType p2 = createVar(BOOL);
//     MyType p3 = createVar(MEDIUM_INT);
//     MyType p4 = createVar(CHAR);
//     usleep(150 * 1000);
//     freeElem(p1);
//     freeElem(p3);
//     MyType p5 = createVar(INT);
//     cout << "p5.addr: " << p5.addr << endl;
//     endScope();
//     initScope();
//     usleep(100 * 1000);
//     p1 = createVar(INT);
//     p2 = createVar(BOOL);
//     p3 = createVar(MEDIUM_INT);
//     p4 = createVar(CHAR);
//     endScope();
//     sleep(100);
//     freeMem();
// }

int main() {
//     createMem(120);

//     // MyType aa = createVar(INT);
//     // aa.print();
//     // assignVar(aa, -19);
//     // int x;
//     // readVar(aa, &x);
//     // DEBUG("value of aa = %d", x);

//     // // int *p = mem->start;
//     // // printf("%d, %d\n", *p >> 1, *p & 1);
//     // // printf("%d, %d\n", *(p + 2) >> 1, *(p + 2) & 1);
//     // // printf("%d, %d\n", *(p + 3) >> 1, *(p + 3) & 1);

//     // MyType a = createVar(CHAR);
//     // a.print();
//     // page_table->pt[counterToIdx(a.ind)].print();
//     // int ret = assignVar(a, 'b');
//     // DEBUG("ret = %d", ret);
//     // char v;
//     // readVar(a, &v);
//     // DEBUG("value of a = %c", v);

//     // MyType b = createVar(BOOLEAN);
//     // b.print();
//     // page_table->pt[counterToIdx(b.ind)].print();
//     // ret = assignVar(b, false);
//     // DEBUG("ret = %d", ret);
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

//     MyType arr1 = createArr(INT, 5);
//     arr1.print();
//     int arr1_val[] = {1, 2, -3, 4, -5};
//     int r1 = assignArr(arr1, arr1_val);
//     DEBUG("ret = %d", r1);
//     int arr1_val_read[5];
//     readArr(arr1, arr1_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr1_val_read[i]);
//     // }
//     assignArr(arr1, 0, -6);
//     assignArr(arr1, 1, -7);
//     assignArr(arr1, 2, -8);
//     assignArr(arr1, 3, -9);
//     assignArr(arr1, 4, 25);
//     readArr(arr1, arr1_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr1_val_read[i]);
//     // }

//     MyType arr = createArr(MEDIUM_INT, 5);
//     arr.print();
//     int arr_val[] = {1, 2, -3, 4, -5};
//     int r = assignArr(arr, arr_val);
//     DEBUG("ret = %d", r);
//     int arr_val_read[5];
//     readArr(arr, arr_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     // }
//     printf("\n");
//     assignArr(arr, 0, -6);
//     assignArr(arr, 1, 7);
//     assignArr(arr, 2, -8);
//     assignArr(arr, 3, 9);
//     assignArr(arr, 4, 25);
//     readArr(arr, arr_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     // }

//     MyType arr2 = createArr(INT, 5);
//     arr2.print();
//     int arr2_val[] = {1, 2, -3, 4, -5};
//     int r2 = assignArr(arr2, arr2_val);
//     DEBUG("ret = %d", r2);
//     int arr2_val_read[5];
//     readArr(arr2, arr2_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr2_val_read[i]);
//     // }
//     assignArr(arr2, 0, -6);
//     assignArr(arr2, 1, -7);
//     assignArr(arr2, 2, -8);
//     assignArr(arr2, 3, -9);
//     assignArr(arr2, 4, 25);
//     readArr(arr2, arr2_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr2_val_read[i]);
//     // }

//     freeElem(arr);

//     MyType arr3 = createArr(CHAR, 5);
//     arr3.print();
//     char arr3_val[] = {'a', 'b', 'c', 'd', 'e'};
//     int r3 = assignArr(arr3, arr3_val);
//     DEBUG("ret = %d", r3);
//     char arr3_val_read[5];
//     readArr(arr3, arr3_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr3_val_read[%zu] = %c", i, arr3_val_read[i]);
//     // }
//     printf("\n");
//     assignArr(arr3, 0, 'f');
//     assignArr(arr3, 1, 'g');
//     assignArr(arr3, 2, 'h');
//     assignArr(arr3, 3, 'i');
//     assignArr(arr3, 4, 'j');
//     readArr(arr3, arr3_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     // DEBUG("arr3_val_read[%zu] = %c", i, arr3_val_read[i]);
//     // }

//     // MyType arr = createArr(BOOLEAN, 5);
//     // arr.print();
//     // bool arr_val[] = {true, false, true, false, true};
//     // int r = assignArr(arr, arr_val);
//     // DEBUG("ret = %d", r);
//     // bool arr_val_read[5];
//     // readArr(arr, arr_val_read);
//     // for (size_t i = 0; i < 5; i++) {
//     //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
//     // }
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

    cleanExit();
}