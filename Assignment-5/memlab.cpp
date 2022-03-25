#include "memlab.h"

#include <pthread.h>

#include <csignal>
#include <cstring>
using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[SUCCESS] " msg " \033[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\033[1;32m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf("\033[1;32m[DEBUG] " msg " \033[0m\n", ##__VA_ARGS__);
#define WARNING(msg, ...) printf("\033[1;33m[WARNING] " msg "\033[0m\n", ##__VA_ARGS__);

typedef unsigned int u_int;

const size_t MAX_PT_ENTRIES = 1024;
const size_t MAX_STACK_SIZE = 1024;

const int MEDIUM_INT_MAX = (1 << 23) - 1;
const int MEDIUM_INT_MIN = -(1 << 23);

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
            return -1;
        }
        DEBUG("start = %lu", start);
        end = start + (bytes >> 2);
        size = bytes >> 2;  // in words
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
        if (p < end) {
            return p;
        } else {
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
        size++;
        if (size < MAX_PT_ENTRIES) {
            head = next;
        } else {
            head = MAX_PT_ENTRIES;
            tail = MAX_PT_ENTRIES;
        }
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
            return -2;
        }
        return st[size - 1];
    }

    int push(int v) {
        if (size == MAX_STACK_SIZE) {
            return -2;
        }
        st[size++] = v;
        return 0;
    }

    int pop() {
        if (size == 0) {
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

int freeElem(MyType &var) {
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
        }
        p = p + (*p >> 1);
    }
}

void updatePageTable() {
    for (int i = 0; i < MAX_PT_ENTRIES; i++) {
        if (page_table->isValidEntry(i)) {
            int *p = mem->getAddr(page_table->pt[i].addr);
            int newAddr = *(p + (*p >> 1) - 1) >> 1;
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
    if (var_stack->push(-1) < 0) {
        throw runtime_error("Stack full, cannot push");
    }
}

void endScope() {
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

void createMem(size_t bytes) {
    mem = (Memory *)malloc(sizeof(Memory));
    if (mem->init(bytes) == -1) {
        throw runtime_error("Memory allocation failed");
    }

    page_table = (PageTable *)malloc(sizeof(PageTable));
    page_table->init();

    var_stack = (Stack *)malloc(sizeof(Stack));
    var_stack->init();

    pthread_t gc_tid;
    pthread_create(&gc_tid, NULL, gcThread, NULL);
}

MyType create(VarType var_type, DataType data_type, u_int len, u_int size_req) {
    LOCK(&mem->mutex);
    int *p = mem->findFreeBlock(size_req);
    if (p == NULL) {
        UNLOCK(&mem->mutex);
        throw runtime_error("No free block in memory");
    }
    mem->allocateBlock(p, size_req);
    UNLOCK(&mem->mutex);

    int addr = mem->getOffset(p);
    LOCK(&page_table->mutex);
    int idx = page_table->insert(addr);
    DEBUG("Inserted in page table at index %d", idx);
    UNLOCK(&page_table->mutex);
    if (idx < 0) {
        throw runtime_error("No free space in page table");
    }
    u_int ind = idxToCounter(idx);
    if (var_stack->push(ind) < 0) {
        throw runtime_error("Stack full, cannot push");
    }
    return MyType(ind, var_type, data_type, len);
}

MyType createVar(DataType type) {
    return create(PRIMITIVE, type, 1, 1);
}

int *checkGetAddr(MyType &var, VarType type) {
    if (var.var_type != type) {
        string str = (type == PRIMITIVE ? "primitive" : "array");
        throw runtime_error("Variable is not a " + str);
    }
    if (!page_table->isValidEntry(counterToIdx(var.ind))) {
        throw runtime_error("Variable is not valid");
    }
    u_int idx = counterToIdx(var.ind);
    int *p = mem->getAddr(page_table->pt[idx].addr) + 1;
    return p;
}

void assignVar(MyType &var, int val) {
    int *p = checkGetAddr(var, PRIMITIVE);
    if (var.data_type == INT) {
        memcpy(p, &val, getSize(INT));
    } else if (var.data_type == MEDIUM_INT) {
        if (val < MEDIUM_INT_MIN || val > MEDIUM_INT_MAX) {
            WARNING("Variable is of type medium int, value is out of range, compressing");
        }
        memcpy(p, &val, getSize(MEDIUM_INT));
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(var.data_type));
    }
}

void assignVar(MyType &var, char val) {
    int *p = checkGetAddr(var, PRIMITIVE);
    if (var.data_type == CHAR) {
        memcpy(p, &val, getSize(CHAR));
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(var.data_type));
    }
}

void assignVar(MyType &var, bool val) {
    int *p = checkGetAddr(var, PRIMITIVE);
    if (var.data_type == BOOLEAN) {
        memcpy(p, &val, getSize(BOOLEAN));
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(var.data_type));
    }
}

void readVar(MyType &var, void *ptr) {
    int *p = checkGetAddr(var, PRIMITIVE);
    int size = getSize(var.data_type);
    memcpy(ptr, p, size);
    if (var.data_type == MEDIUM_INT) {
        int num_val = *(int *)ptr;
        if (num_val & (1 << 23)) {
            num_val = num_val | 0xff000000;
        }
        memcpy(ptr, &num_val, 4);
    }
}

MyType createArr(DataType type, int len) {
    if (len <= 0) {
        throw runtime_error("Length of array should be greater than 0");
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
    int *p = checkGetAddr(arr, ARRAY);
    if (arr.data_type == INT) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy(p + i, &val[i], getSize(INT));
        }
    } else if (arr.data_type == MEDIUM_INT) {
        for (size_t i = 0; i < arr.len; i++) {
            if (val[i] < MEDIUM_INT_MIN || val[i] > MEDIUM_INT_MAX) {
                WARNING("Variable at index %zu is of type medium int, value is out of range, compressing", i);
            }
            memcpy(p + i, &val[i], getSize(MEDIUM_INT));
        }
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    }
}

void assignArr(MyType &arr, char val[]) {
    int *p = checkGetAddr(arr, ARRAY);
    if (arr.data_type == CHAR) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy((char *)p + i, &val[i], getSize(CHAR));
        }
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    }
}

void assignArr(MyType &arr, bool val[]) {
    int *p = checkGetAddr(arr, ARRAY);
    if (arr.data_type == BOOLEAN) {
        for (size_t i = 0; i < arr.len; i++) {
            memcpy((char *)p + i, &val[i], getSize(BOOLEAN));
        }
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    }
}

void assignArr(MyType &arr, int index, int val) {
    int *p = checkGetAddr(arr, ARRAY);
    if (index < 0 || index >= arr.len) {
        throw runtime_error("Index out of range");
    }
    if (arr.data_type == INT) {
        memcpy(p + index, &val, getSize(INT));
    } else if (arr.data_type == MEDIUM_INT) {
        if (val < MEDIUM_INT_MIN || val > MEDIUM_INT_MAX) {
            WARNING("Variable at index %d is of type medium int, value is out of range, compressing", index);
        }
        memcpy(p + index, &val, getSize(MEDIUM_INT));
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    }
}

void assignArr(MyType &arr, int index, char val) {
    int *p = checkGetAddr(arr, ARRAY);
    if (index < 0 || index >= arr.len) {
        throw runtime_error("Index out of range");
    }
    if (arr.data_type == CHAR) {
        memcpy((char *)p + index, &val, getSize(CHAR));
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    }
}

void assignArr(MyType &arr, int index, bool val) {
    int *p = checkGetAddr(arr, ARRAY);
    if (index < 0 || index >= arr.len) {
        throw runtime_error("Index out of range");
    }
    if (arr.data_type == BOOLEAN) {
        memcpy((char *)p + index, &val, getSize(BOOLEAN));
    } else {
        throw runtime_error("Type mismatch. Data type of variable is " + getDataTypeStr(arr.data_type));
    }
}

void readArr(MyType &arr, void *ptr) {
    int *p = checkGetAddr(arr, ARRAY);
    int size = getSize(arr.data_type);
    for (size_t i = 0; i < arr.len; i++) {
        if (arr.data_type == INT) {
            memcpy((int *)ptr + i, p + i, size);
        } else if (arr.data_type == MEDIUM_INT) {
            int num_val = *(int *)(p + i);
            if (num_val & (1 << 23)) {
                num_val = num_val | 0xff000000;
            }
            memcpy((int *)ptr + i, &num_val, 4);
        } else if (arr.data_type == CHAR) {
            memcpy((char *)ptr + i, (char *)p + i, size);
        } else if (arr.data_type == BOOLEAN) {
            memcpy((bool *)ptr + i, (char *)p + i, size);
        }
    }
}

void readArr(MyType &arr, int index, void *ptr) {
    int *p = checkGetAddr(arr, ARRAY);
    if (index < 0 || index >= arr.len) {
        throw runtime_error("Index out of range");
    }
    int size = getSize(arr.data_type);
    if (arr.data_type == INT) {
        memcpy(ptr, p + index, size);
    } else if (arr.data_type == MEDIUM_INT) {
        int num_val = *(int *)(p + index);
        if (num_val & (1 << 23)) {
            num_val = num_val | 0xff000000;
        }
        memcpy(ptr, &num_val, 4);
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
    createMem(120);

    // MyType aa = createVar(INT);
    // aa.print();
    // assignVar(aa, -19);
    // int x;
    // readVar(aa, &x);
    // DEBUG("value of aa = %d", x);

    // int *p = mem->start;
    // printf("%d, %d\n", *p >> 1, *p & 1);
    // printf("%d, %d\n", *(p + 2) >> 1, *(p + 2) & 1);
    // printf("%d, %d\n", *(p + 3) >> 1, *(p + 3) & 1);

    // MyType a = createVar(CHAR);
    // a.print();
    // page_table->pt[counterToIdx(a.ind)].print();
    // assignVar(a, 'b');
    // char v;
    // readVar(a, &v);
    // DEBUG("value of a = %c", v);

    // MyType b = createVar(BOOLEAN);
    // b.print();
    // page_table->pt[counterToIdx(b.ind)].print();
    // assignVar(b, false);
    // bool xx;
    // readVar(b, &xx);
    // // cout << xx << endl;
    // DEBUG("value of b = %d", xx);

    // MyType bb = createVar(MEDIUM_INT);
    // bb.print();
    // assignVar(bb, 19);
    // int y;
    // readVar(bb, &y);
    // DEBUG("value of bb = %d", y);

    // MyType arr1 = createArr(INT, 5);
    // arr1.print();
    // int arr1_val[] = {1, 2, -3, 4, -5};
    // assignArr(arr1, arr1_val);
    // int arr1_val_read[5];
    // readArr(arr1, arr1_val_read);
    // for (size_t i = 0; i < 5; i++) {
    //     DEBUG("arr_val_read[%zu] = %d", i, arr1_val_read[i]);
    // }
    // assignArr(arr1, 0, -6);
    // assignArr(arr1, 1, -7);
    // assignArr(arr1, 2, -8);
    // assignArr(arr1, 3, -9);
    // assignArr(arr1, 4, 25);
    // readArr(arr1, arr1_val_read);
    // for (size_t i = 0; i < 5; i++) {
    //     DEBUG("arr_val_read[%zu] = %d", i, arr1_val_read[i]);
    // }
    // int v;
    // readArr(arr1, 0, &v);
    // DEBUG("%d", v);
    // readArr(arr1, 1, &v);
    // DEBUG("%d", v);
    // readArr(arr1, 2, &v);
    // DEBUG("%d", v);
    // readArr(arr1, 3, &v);
    // DEBUG("%d", v);
    // readArr(arr1, 4, &v);
    // DEBUG("%d", v);

    // MyType arr = createArr(MEDIUM_INT, 5);
    // arr.print();
    // int arr_val[] = {1, 2, -3, 4, -5};
    // assignArr(arr, arr_val);
    // int arr_val_read[5];
    // readArr(arr, arr_val_read);
    // for (size_t i = 0; i < 5; i++) {
    //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
    // }
    // printf("\n");
    // assignArr(arr, 0, -6);
    // assignArr(arr, 1, 7);
    // assignArr(arr, 2, -8);
    // assignArr(arr, 3, 9);
    // assignArr(arr, 4, 25);
    // readArr(arr, arr_val_read);
    // for (size_t i = 0; i < 5; i++) {
    //     DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
    // }
    // int v;
    // readArr(arr, 0, &v);
    // DEBUG("%d", v);
    // readArr(arr, 1, &v);
    // DEBUG("%d", v);
    // readArr(arr, 2, &v);
    // DEBUG("%d", v);
    // readArr(arr, 3, &v);
    // DEBUG("%d", v);
    // readArr(arr, 4, &v);
    // DEBUG("%d", v);

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

    // MyType arr3 = createArr(CHAR, 5);
    // arr3.print();
    // char arr3_val[] = {'a', 'b', 'c', 'd', 'e'};
    // assignArr(arr3, arr3_val);
    // char arr3_val_read[5];
    // readArr(arr3, arr3_val_read);
    // for (size_t i = 0; i < 5; i++) {
    //     DEBUG("arr3_val_read[%zu] = %c", i, arr3_val_read[i]);
    // }
    // printf("\n");
    // assignArr(arr3, 0, 'f');
    // assignArr(arr3, 1, 'g');
    // assignArr(arr3, 2, 'h');
    // assignArr(arr3, 3, 'i');
    // assignArr(arr3, 4, 'j');
    // readArr(arr3, arr3_val_read);
    // for (size_t i = 0; i < 5; i++) {
    //     DEBUG("arr3_val_read[%zu] = %c", i, arr3_val_read[i]);
    // }

    MyType arr = createArr(BOOLEAN, 5);
    arr.print();
    bool arr_val[] = {true, false, true, false, true};
    assignArr(arr, arr_val);
    bool arr_val_read[5];
    readArr(arr, arr_val_read);
    for (size_t i = 0; i < 5; i++) {
        DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
    }
    printf("\n");
    assignArr(arr, 0, false);
    assignArr(arr, 1, true);
    assignArr(arr, 2, false);
    assignArr(arr, 3, true);
    assignArr(arr, 4, false);
    readArr(arr, arr_val_read);
    for (size_t i = 0; i < 5; i++) {
        DEBUG("arr_val_read[%zu] = %d", i, arr_val_read[i]);
    }

    cleanExit();
}