#include <pthread.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[SUCCESS] " msg " \033[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\033[1;32m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define PROMPT(msg, ...) printf("\033[1;33m[PROMPT]" msg "\033[0m", ##__VA_ARGS__);

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

struct MyType {
    int ind;
    VarType var_type;
    DataType data_type;
    int num_elements;
};

struct PageTableEntry {
    unsigned int entry;
    /* To be merged in entry itself */
    // bool valid;
    // bool marked;

    int getAddr() {
        return (entry >> 2);
    }

    void setAddr(int addr) {
        entry = (entry & 3) | (addr << 2);
    }

    int getValidBit() {
        return ((entry >> 1) & 1);
    }

    void setValidBit(int bit) {
        entry = (entry & ~(1 << 1)) | (bit << 1);
    }

    int getMarkedBit() {
        return (entry & 1);
    }

    void setMarkedBit(int bit) {
        entry = (entry & ~1) | bit;
    }

    void init() {
        setAddr(0);
        setValidBit(0);
        setMarkedBit(0);
    }
};

struct PageTable {
    PageTableEntry entries[MAX_PT_ENTRIES];
    pthread_mutex_t mutex;

    void init() {
        for (int i = 0; i < MAX_PT_ENTRIES; i++) {
            entries[i].init();
        }
        pthread_mutex_init(&mutex, NULL);
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
void *mem;
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

int createMem(long long bytes) {
    mem = malloc(bytes);
    if (mem == NULL) {
        return -1;
    }

    // TODO: set up inital free list of size total size

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
    // TODO: Test PageTableEntry functions
    PageTableEntry pte;
}