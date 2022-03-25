#include <bits/stdc++.h>
#include <unistd.h>

#include "memlab.h"
using namespace std;

void randArr(MyType x, MyType y) {
    cout << "creating random array" << endl;
    MyType arr = createArr(x.data_type, 50000);
    cout << "array created" << endl;
    for (int i = 0; i < 500; i++) {
        initScope();
        int r = rand() % 26;
        MyType p1 = createVar(INT);
        assignVar(p1, r);
        readVar(p1, &r);
        char c = 'a' + r;
        MyType p2 = createVar(CHAR);
        assignVar(p2, c);
        readVar(p2, &c);
        assignArr(arr, i, c);
        endScope();
    }
    freeElem(arr);
}

int main() {
    createMem(250 * 1024 * 1024);  // 250MB
    for (int i = 0; i < 10; i++) {
        cout << "called for " << i << endl;
        initScope();
        MyType x = createVar(CHAR);
        MyType y = createVar(CHAR);
        randArr(x, y);
        cout << "done" << i << endl;
        endScope();
        usleep(50 * 1000);
    }
    sleep(10);
    cleanExit();
}