#ifndef __COMMAND_H
#define __COMMAND_H

#include <iostream>
#include <string>
#include <vector>

using namespace std;

class Command {
   public:
    string cmd;
    vector<string> args;
    int fd_in, fd_out;
    string input_file, output_file;
    pid_t pid;

    Command(const string& cmd);
    ~Command();

    void parse();
    void io_redirect();
    friend ostream& operator<<(ostream& os, const Command& cmd);
};

#endif