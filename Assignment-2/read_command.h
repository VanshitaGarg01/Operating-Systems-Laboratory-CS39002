#ifndef __READ_COMMAND_H
#define __READ_COMMAND_H

#include <iostream>
#include <string>
#include <vector>

#include "Command.h"
#include "utility.h"
using namespace std;

string readCommand() {
    string command;
    getline(cin, command);
    if (cin.bad()) {
        cin.clear();
        cout << endl;
        return "";
    }
    // cout << command << " " << command.length() << endl;
    return command;
    // getline();
    // currently may just read string using getline, however we will shift to termios, char by char
    // return the command as it is as read
}

Pipeline* getCommand() {
    // cout << "Assignment-2>> ";
    PROMPT("vash>> ");
    string cmd = readCommand();
    vector<string> piped_cmds = split(cmd, '|');
    // cout << piped_cmds.size() << endl;
    vector<Command*> cmds;

    bool flag = 1;
    for (auto cmd_str : piped_cmds) {
        Command* c = new Command(cmd_str);
        if (c->parse()) {
            cmds.push_back(c);
        } else {
            flag = 0;
            break;
        }
    }
    if (flag) {
        return new Pipeline(cmds);
    } else {
        return new Pipeline(-1);
    }
}

#endif