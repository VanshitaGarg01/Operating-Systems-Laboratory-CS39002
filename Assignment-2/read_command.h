#ifndef __READ_COMMAND_H
#define __READ_COMMAND_H

#include <string>
#include <vector>

#include "Command.h"
#include "utility.h"
using namespace std;

string readCommand() {
    // getline();
    // currently may just read string using getline, however we will shift to termios, char by char
    // return the command as it is as read
}

vector<Command*> getCommand() {
    // print prompt
    // readCommand();
    // split into pipelines using split with | -> vector<string>
    // iterate on each string, create object, parse, return vector<Command*>
}

#endif