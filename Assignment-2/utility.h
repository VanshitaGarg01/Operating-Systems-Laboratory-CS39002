#ifndef __UTILITY_H
#define __UTILITY_H

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf("\033[1;34m[DEBUG] " msg " \033[0m\n", ##__VA_ARGS__);
#define LOG(msg, ...) printf("\033[1;34m[LOG] " msg " \033[0m\n", ##__VA_ARGS__);
#define PROMPT(msg, ...) printf("\033[1;32m" msg "\033[0m", ##__VA_ARGS__);

vector<string> split(string& str, char delim) {
    vector<string> tokens;
    stringstream ss(str);
    string tmp;
    while (getline(ss, tmp, delim)) {
        tokens.push_back(tmp);
    }
    return tokens;
}

vector<char*> cstrArray(vector<string>& args) {
    vector<char*> args_(args.size() + 1);
    for (int i = 0; i < args.size(); i++) {
        args_[i] = (char*)malloc((args[i].length() + 1) * sizeof(char));
        strcpy(args_[i], args[i].c_str());
    }
    args_[args.size()] = (char*)malloc(sizeof(char));
    args_[args.size()] = nullptr;
    // strcpy(args_[args.size()], "\0");
    return args_;
}

#endif