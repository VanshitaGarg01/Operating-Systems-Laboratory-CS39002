#include <iostream>
using namespace std;

int main() {
    // char *mem = (char *)malloc(24);
    // *(int *)mem = 5;
    // // printf("%d\n", *(int *)mem);
    // *(int *)(mem + 4) = 6;
    // for (int i = 0; i < 8; i += 4) {
    //     printf("%d\n", *(int *)(mem + i));
    // }
    createMem(1024);
    printf("mem->start = %lu\n", mem->start);
    printf("mem->end = %lu\n", mem->end);
    printf("%d, %d\n\n", *mem->start >> 1, *mem->start & 1);

    int *p = mem->findFreeBlock(8);
    printf("%lu\n", p);
    assert(p == mem->start);
    mem->allocateBlock(p, 8);
    printf("%d, %d\n", *p >> 1, *p & 1);
    printf("%d, %d\n", *(p + 9) >> 1, *(p + 9) & 1);
    printf("%d, %d\n\n", *(p + 10) >> 1, *(p + 10) & 1);

    int *q = mem->findFreeBlock(10);
    printf("%lu\n", q);
    mem->allocateBlock(q, 10);
    printf("%d, %d\n", *q >> 1, *q & 1);
    printf("%d, %d\n", *(q + 11) >> 1, *(q + 11) & 1);
    printf("%d, %d\n\n", *(q + 12) >> 1, *(q + 12) & 1);

    int *r = mem->findFreeBlock(6);
    printf("%lu\n", r);
    mem->allocateBlock(r, 6);
    printf("%d, %d\n", *r >> 1, *r & 1);
    printf("%d, %d\n", *(r + 7) >> 1, *(r + 7) & 1);
    printf("%d, %d\n\n", *(r + 8) >> 1, *(r + 8) & 1);

    mem->freeBlock(r);
    printf("%d, %d\n", *r >> 1, *r & 1);
    printf("%d, %d\n\n", *(mem->end - 1) >> 1, *(mem->end - 1) & 1);

    mem->freeBlock(p);
    printf("%d, %d\n", *p >> 1, *p & 1);
    printf("%d, %d\n\n", *(p + 9) >> 1, *(p + 9) & 1);

    mem->freeBlock(q);
    printf("%d, %d\n", *mem->start >> 1, *mem->start & 1);
    printf("%d, %d\n\n", *(mem->end - 1) >> 1, *(mem->end - 1) & 1);
}

/*
head, tail
remove from head
add free block to tail
*/

// 0 0 0 5 0 0 0 6