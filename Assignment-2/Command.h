#ifndef __COMMAND_H
#define __COMMAND_H

#include <fcntl.h>
#include <sys/types.h>
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
        if (input_file != "") {
            fd_in = open(input_file.c_str(), O_RDONLY);
            if (fd_in < 0) {
                perror("open");
                exit(1);
            }
        }
        int ret = dup2(fd_in, STDIN_FILENO);
        if (ret < 0) {
            perror("dup2");
            exit(1);
        }

        if (output_file != "") {
            fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) {
                perror("open");
                exit(1);
            }
        }
        ret = dup2(fd_out, STDOUT_FILENO);
        if (ret < 0) {
            perror("dup2");
            exit(1);
        }
    }
};

#endif