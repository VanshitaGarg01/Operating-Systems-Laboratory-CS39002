#ifndef __HEADER_H
#define __HEADER_H

#include <iostream>
#include <string>
#include <vector>

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
#define PROMPT(msg, ...) printf(COLOR_GREEN msg COLOR_RESET "\n", ##__VA_ARGS__);

#define DIR_LENGTH 1000

#define HIST_SIZE 10000
#define HIST_DISPLAY_SIZE 1000
#define HIST_FILE ".shell_history"

#define RUNNING 0
#define STOPPED 1
#define DONE 2

/* Command.cpp */
class Command {
   public:
    string cmd;
    vector<string> args;
    int fd_in, fd_out;
    string input_file, output_file;
    bool is_bg;
    pid_t pid;

    Command(const string& cmd);
    ~Command();

    int parse();
    void io_redirect();
    friend ostream& operator<<(ostream& os, const Command& cmd);
};

class Pipeline {
   public:
    vector<Command*> cmds;
    pid_t pgid;
    int num_active;
    int status;

    Pipeline(int num_p);
    Pipeline(vector<Command*>& cmds);
    friend ostream& operator<<(ostream& os, const Pipeline& p);
};

/* utility.cpp */
vector<string> split(string& str, char delim);
vector<char*> cstrArray(vector<string>& args);

/* read_command.cpp */
void displayPrompt();
string readCommand();
Pipeline* getCommand(string cmd);

/* history.cpp */
void loadHistory();
string searchInHistory(string s);
void printHistory();
void addToHistory(string s);
void updateHistory();

/* autocomplete.cpp */
vector<string> getFilesInCurrDir();
vector<string> autocomplete(string s);

/* shell.cpp */

#endif