#include <fcntl.h>
#include <unistd.h>

#include <map>

#include "header.h"
using namespace std;

Command::Command(const string& cmd) : cmd(cmd), fd_in(STDIN_FILENO), fd_out(STDOUT_FILENO), input_file(""), output_file(""), is_bg(false) {}

Command::~Command() {
    if (fd_in != STDIN_FILENO) {
        close(fd_in);
    }
    if (fd_out != STDOUT_FILENO) {
        close(fd_out);
    }
}

int Command::parse() {
    // initially, cmd can be  "   man    find   "
    // need to extract <, >, &
    // replace cmd with "man find"
    // can throw exceptions which should be caught and handled

    vector<string> tokens;
    // cout << "cmd is " << cmd << endl;
    string temp = "";
    for (size_t i = 0; i < cmd.length(); i++) {
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
    for (size_t i = 0; i < tokens.size(); i++) {
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

void Command::io_redirect() {
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

ostream& operator<<(ostream& os, const Command& cmd) {
    for (auto it : cmd.args) {
        os << it << " ";
    }
    cout << "fd_in: " << cmd.fd_in << " fd_out: " << cmd.fd_out << endl;
    cout << "file in: " << cmd.input_file << " file out: " << cmd.output_file << endl;
    cout << "is_bg: " << cmd.is_bg << endl;
    return os;
}
