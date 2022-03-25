#include <cstring>
#include <iostream>

// #include "t.h"
#define STACK(msg, ...) printf("\033[1;38m[STACK] " msg "\033[0m\n", ##__VA_ARGS__);

using namespace std;

struct T {
    const int a;
    int b;

    T(int _a) : a(_a), b(0) {}
};

enum temp {
    a,
    b,
    c
};

int main() {
    char *mem = (char *)malloc(24);
    *(int *)mem = 5;
    // printf("%d\n", *(int *)mem);
    *(int *)(mem + 4) = 6;
    for (int i = 0; i < 8; i += 1) {
        printf("%d\n", *(char *)(mem + i));
    }

    // createMem(1024);
    // printf("mem->start = %lu\n", mem->start);
    // printf("mem->end = %lu\n", mem->end);
    // printf("%d, %d\n\n", *mem->start >> 1, *mem->start & 1);

    // int *p = mem->findFreeBlock(8);
    // printf("%lu\n", p);
    // assert(p == mem->start);
    // mem->allocateBlock(p, 8);
    // printf("%d, %d\n", *p >> 1, *p & 1);
    // printf("%d, %d\n", *(p + 9) >> 1, *(p + 9) & 1);
    // printf("%d, %d\n\n", *(p + 10) >> 1, *(p + 10) & 1);

    // int *q = mem->findFreeBlock(10);
    // printf("%lu\n", q);
    // mem->allocateBlock(q, 10);
    // printf("%d, %d\n", *q >> 1, *q & 1);
    // printf("%d, %d\n", *(q + 11) >> 1, *(q + 11) & 1);
    // printf("%d, %d\n\n", *(q + 12) >> 1, *(q + 12) & 1);

    // int *r = mem->findFreeBlock(6);
    // printf("%lu\n", r);
    // mem->allocateBlock(r, 6);
    // printf("%d, %d\n", *r >> 1, *r & 1);
    // printf("%d, %d\n", *(r + 7) >> 1, *(r + 7) & 1);
    // printf("%d, %d\n\n", *(r + 8) >> 1, *(r + 8) & 1);

    // mem->freeBlock(r);
    // printf("%d, %d\n", *r >> 1, *r & 1);
    // printf("%d, %d\n\n", *(mem->end - 1) >> 1, *(mem->end - 1) & 1);

    // mem->freeBlock(p);
    // printf("%d, %d\n", *p >> 1, *p & 1);
    // printf("%d, %d\n\n", *(p + 9) >> 1, *(p + 9) & 1);

    // mem->freeBlock(q);
    // printf("%d, %d\n", *mem->start >> 1, *mem->start & 1);
    // printf("%d, %d\n\n", *(mem->end - 1) >> 1, *(mem->end - 1) & 1);

    // T st(5);
    // st.b = 5;
    // st.a = 3;
    // cout << st.a << " " << st.b << endl;

    // int *p = (int *)malloc(24);
    // int v = -12;
    // memcpy(p, &v, 3);
    // memset((char *)p + 3, -1, 1);
    // int x;
    // memcpy(&x, p, 4);

    // cout << x << endl;
    // int *p = (int *)malloc(sizeof(int));
    // unsigned int x = p;
    // cout << x << endl;

    // temp ex = a;

    STACK("hello");

}

/*
head, tail
remove from head
add free block to tail
*/

// 0 0 0 5 0 0 0 6