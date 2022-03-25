#ifndef __MEMLAB_H
#define __MEMLAB_H

#include <unistd.h>

#include <iostream>
#include <string>

using namespace std;

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

inline int getSize(DataType data_type) {
    if (data_type == INT) {
        return 4;
    } else if (data_type == CHAR) {
        return 1;
    } else if (data_type == MEDIUM_INT) {
        return 3;
    } else if (data_type == BOOLEAN) {
        return 1;
    } else {
        return -1;
    }
}

inline string getDataTypeStr(DataType data_type) {
    if (data_type == INT) {
        return "int";
    } else if (data_type == CHAR) {
        return "char";
    } else if (data_type == MEDIUM_INT) {
        return "medium int";
    } else if (data_type == BOOLEAN) {
        return "boolean";
    } else {
        return "unknown";
    }
}

struct MyType {
    const int ind;
    const VarType var_type;
    const DataType data_type;
    const size_t len;

    MyType(int _ind, VarType _var_type, DataType _data_type, size_t _len) : ind(_ind), var_type(_var_type), data_type(_data_type), len(_len) {}

    void print() {
        printf("MyType: %d %s %s %zu\n", ind, getDataTypeStr(data_type).c_str(), var_type == PRIMITIVE ? "primitive" : "array", len);
    }
};

void createMem(size_t bytes);

MyType createVar(DataType type);
void assignVar(MyType &var, int val);
void assignVar(MyType &var, char val);
void assignVar(MyType &var, bool val);
void readVar(MyType &var, void *ptr);

MyType createArr(DataType type, int len);
void assignArr(MyType &arr, int val[]);
void assignArr(MyType &arr, char val[]);
void assignArr(MyType &arr, bool val[]);
void assignArr(MyType &arr, int index, int val);
void assignArr(MyType &arr, int index, char val);
void assignArr(MyType &arr, int index, bool val);
void readArr(MyType &arr, void *ptr);
void readArr(MyType &arr, int index, void *ptr);

int freeElem(MyType var);
void gcRun();

void initScope();
void endScope();

void cleanExit();

#endif