#include <iostream>
using namespace std;

#define RED "\x1b[1;31m"
#define GREEN "\x1b[1;32m"
#define YELLOW "\x1b[1;33m"
#define BLUE "\x1b[1;34m"
#define MAGENTA "\x1b[1;35m"
#define CYAN "\x1b[1;36m"
#define WHITE "\x1b[1;37m"
#define RESET "\x1b[0m"

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

int main() {
    ERROR("test string check");
    WARNING("test string check");
    LIBRARY("test string check");
    PAGE_TABLE("test string check");
    WORD_ALIGN("test string check");
    GC("test string check");
    STACK("test string check");
    MEMORY("test string check");
}