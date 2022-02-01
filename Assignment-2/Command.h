#ifndef __COMMAND_H
#define __COMMAND_H

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <vector>

#include "utility.h"
using namespace std;

#define RUNNING 0
#define STOPPED 1
#define DONE 2

class Command {
   public:
    string cmd;
    vector<string> args;
    int fd_in, fd_out;
    string input_file, output_file;
    bool is_bg;
    pid_t pid;

    Command(const string& cmd) : cmd(cmd), fd_in(STDIN_FILENO), fd_out(STDOUT_FILENO), input_file(""), output_file(""), is_bg(false) {}

    ~Command() {
        if (fd_in != STDIN_FILENO) {
            close(fd_in);
        }
        if (fd_out != STDOUT_FILENO) {
            close(fd_out);
        }
    }

    int parse() {
        // initially, cmd can be  "   man    find   "
        // need to extract <, >, &
        // replace cmd with "man find"
        // can throw exceptions which should be caught and handled

        vector<string> tokens;
        // cout << "cmd is " << cmd << endl;
        string temp = "";
        for (int i = 0; i < cmd.length(); i++) {
            if (cmd[i] == '\\') {
                i++;
                if (i != cmd.length()) {
                    temp += cmd[i];
                } else {
                    return 0;
                }
                continue;
            }
            if (cmd[i] == '>') {
                if (temp.size() > 0) {
                    tokens.push_back(temp);
                    temp = "";
                }
                tokens.push_back(">");
            } else if (cmd[i] == '<') {
                if (temp.size() > 0) {
                    tokens.push_back(temp);
                    temp = "";
                }
                tokens.push_back("<");
            } else if (cmd[i] == '"') {
                i++;
                while (i < cmd.length() && (cmd[i] != '"' || (cmd[i] == '"' && cmd[i - 1] == '\\'))) {
                    temp += cmd[i++];
                }
                if (i == cmd.length()) {
                    return 0;
                }
            } else if (cmd[i] == '&') {
                if (temp.size() > 0) {
                    tokens.push_back(temp);
                    temp = "";
                }
                tokens.push_back("&");
            } else if (cmd[i] == ' ') {
                if (temp.size() > 0) {
                    tokens.push_back(temp);
                    temp = "";
                }
            } else {
                temp += cmd[i];
            }
        }
        if (temp.size() > 0) {
            tokens.push_back(temp);
        }

        // for (auto it: tokens)   cout << it << endl;
        for (int i = 0; i < tokens.size(); i++) {
            if (tokens[i] == "<") {
                i++;
                if (i == tokens.size() || tokens[i] == ">" || tokens[i] == "<" || tokens[i] == "&") {
                    return 0;
                } else {
                    input_file = tokens[i];
                }
            } else if (tokens[i] == ">") {
                i++;
                if (i == tokens.size() || tokens[i] == ">" || tokens[i] == "<" || tokens[i] == "&") {
                    return 0;
                } else {
                    output_file = tokens[i];
                }
            } else if (tokens[i] == "&") {
                i++;
                if (i != tokens.size()) {
                    return 0;
                } else {
                    is_bg = 1;
                }
            } else {
                args.push_back(tokens[i]);
            }
        }

        return 1;
    }

    void io_redirect() {
        if (input_file != "") {
            fd_in = open(input_file.c_str(), O_RDONLY);
            if (fd_in < 0) {
                perror("open");
                exit(1);
            }
            int ret = dup2(fd_in, STDIN_FILENO);
            if (ret < 0) {
                perror("dup2");
                exit(1);
            }
        }

        if (output_file != "") {
            fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) {
                perror("open");
                exit(1);
            }
            int ret = dup2(fd_out, STDOUT_FILENO);
            if (ret < 0) {
                perror("dup2");
                exit(1);
            }
        }
    }

    friend ostream& operator<<(ostream& os, const Command& cmd) {
        for (auto it : cmd.args) {
            os << it << " ";
        }
        cout << "fd_in: " << cmd.fd_in << " fd_out: " << cmd.fd_out << endl;
        cout << "file in: " << cmd.input_file << " file out: " << cmd.output_file << endl;
        cout << "is_bg: " << cmd.is_bg << endl;
        return os;
    }
};

class Pipeline {
   public:
    vector<Command*> cmds;
    pid_t pgid;
    int num_active;
    int status;

    Pipeline(int num_p) : num_active(num_p), status(RUNNING) {}

    Pipeline(vector<Command*>& cmds) : cmds(cmds), pgid(0), num_active(cmds.size()), status(RUNNING) {}

    friend ostream& operator<<(ostream& os, const Pipeline& p) {
        cout << p.pgid << endl;
        for (auto it : p.cmds) {
            os << *it << endl;
        }
        return os;
    }
};

#endif