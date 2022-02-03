#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <bits/stdc++.h>
using namespace std;

#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_RESET "\033[0m"

#define ERROR(msg, ...) printf(COLOR_RED "[ERROR] " msg COLOR_RESET "\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf(COLOR_CYAN "[INFO] " msg COLOR_RESET "\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf(COLOR_BLUE "[DEBUG] " msg COLOR_RESET "\n", ##__VA_ARGS__);
#define LOG(msg, ...) printf(COLOR_BLUE "[LOG] " msg COLOR_RESET "\n", ##__VA_ARGS__);
#define PROMPT(msg, ...) printf(COLOR_GREEN msg COLOR_RESET  "\n", ##__VA_ARGS__);


int main() {
    string s;
    cin >> s;
}
