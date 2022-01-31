#ifndef __COMMAND_H
#define __COMMAND_H

#include <unistd.h>

#include <string>
#include <vector>

#include "utility.h"
using namespace std;

class Command {
   public:
    string cmd;
    vector<string> args;
    int fd_in, fd_out;
    string input_file, output_file;
    bool is_bg;

    Command(const string& cmd) : cmd(cmd), fd_in(STDIN_FILENO), fd_out(STDOUT_FILENO), input_file(""), output_file(""), is_bg(false) {}

    ~Command() {
        if (fd_in != STDIN_FILENO) {
            close(fd_in);
        }
        if (fd_out != STDOUT_FILENO) {
            close(fd_out);
        }
    }

    void parse() {
        // initially, cmd can be  "   man    find   "
        // need to extract <, >, &
        // replace cmd with "man find"
        // can throw exceptions which should be caught and handled
    }

    void io_redirect() {
        
    }
};

#endif