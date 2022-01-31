#ifndef __UTILITY_H
#define __UTILITY_H

#include <string>
#include <vector>

using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] "msg" \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[INFO] "msg" \033[0m\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf("\033[1;34m[DEBUG] "msg" \033[0m\n", ##__VA_ARGS__);
#define LOG(msg, ...) printf("\033[1;34m[LOG] "msg" \033[0m\n", ##__VA_ARGS__);
#define PROMPT(msg, ...) printf("\033[1;32m"msg"\033[0m", ##__VA_ARGS__);

vector<string> split(string& str, char delim) {
}

vector<char*> cstrArray(vector<string>& args) {
}

#endif