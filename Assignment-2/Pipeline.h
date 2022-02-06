#ifndef __PIPELINE_H
#define __PIPELINE_H

#include <string>
#include <vector>

#include "Command.h"

using namespace std;

#define RUNNING 0
#define STOPPED 1
#define DONE 2

class Pipeline {
   public:
    string cmd;
    vector<Command*> cmds;
    bool is_bg;
    pid_t pgid;
    int num_active;
    int status;

    Pipeline(string& cmd);
    Pipeline(vector<Command*>& cmds);
    void parse();
    void executePipeline(bool isMultiwatch = false);
    friend ostream& operator<<(ostream& os, const Pipeline& p);
};

#endif