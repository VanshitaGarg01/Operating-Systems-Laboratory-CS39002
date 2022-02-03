#include <signal.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <deque>
#include <map>

#include "header.h"

using namespace std;

bool ctrlC = 0, ctrlZ = 0, ctrlD = 0;
pid_t fgpid = 0;

deque<string> history;
vector<Pipeline*> all_pipelines;
map<pid_t, int> ind;

bool shellCd(string arg) {
    if (arg.size() == 0) {
        return false;
    }
    if (chdir(arg.c_str()) < 0) {
        return false;
    }
    return true;
}

bool shellExit(string arg = "") {
    updateHistory();
    exit(0);
    return true;
}

bool shellJobs(string arg = "") {
    for (int i = 0; i < (int)all_pipelines.size(); i++) {
        cout << "pgid: " << all_pipelines[i]->pgid << ": ";
        int status = all_pipelines[i]->status;
        if (status == RUNNING) {
            cout << "Running";
        } else if (status == STOPPED) {
            cout << "Stopped";
        } else if (status == DONE) {
            cout << "Done";
        }
        cout << endl;

        vector<Command*> cmds = all_pipelines[i]->cmds;
        for (int j = 0; j < (int)cmds.size(); j++) {
            cout << "--- pid: " << cmds[j]->pid << " " << cmds[j]->cmd << endl;
        }
    }
    return true;
}

vector<string> builtins = {"cd", "exit", "jobs"};
bool (*builtin_funcs[])(string args) = {&shellCd, &shellExit, &shellJobs};

bool handleBuiltin(Pipeline& p) {
    string cmd = p.cmds[0]->args[0];
    for (int i = 0; i < (int)builtins.size(); i++) {
        if (cmd == builtins[i]) {
            string arg = (p.cmds[0]->args.size() > 1) ? p.cmds[0]->args[1] : "";
            return (*builtin_funcs[i])(arg);
        }
    }
    return false;
}

int main() {
    // perform bootup tasks, like loading history into deque
    loadHistory();

    // pid_t shid = getpid();

    // add signal handlers
    struct sigaction action;
    action.sa_handler = CZ_handler;  // SIG_IGN willignore the interrupt and getchar will be blocked
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);
    signal(SIGTTOU, SIG_IGN);

    // https://web.archive.org/web/20170701052127/https://www.usna.edu/Users/cs/aviv/classes/ic221/s16/lab/10/lab.html
    signal(SIGCHLD, reapProcesses);

    while (!ctrlD) {  // add global variable
        displayPrompt();

        string cmd = readCommand();
        if (cmd == "") {
            continue;
        }
        // need to add command to history
        addToHistory(cmd);

        string output_file = "";
        vector<Pipeline*> pList = parseMultiWatch(cmd, output_file);

        // cout << pList.size() << endl;
        // cout << output_file << endl;
        // for (auto& p : pList) {
        //     if (p != NULL) {
        //         cout << *p << endl;
        //     }
        // }
        // if (pList.size() == 1 && pList[0] == NULL) {
        //     DEBUG("here");
        //     continue;
        // }
        // continue;

        // if (pList.size() == 1 && pList[0] == NULL) {
        //     // DEBUG("here");
        //     cout << "Error while parsing command" << endl;
        //     continue;
        // } else 
        if (pList.size() > 0 && pList[0] != NULL) {
            struct sigaction multiWatch_action;
            multiWatch_action.sa_handler = multiWatch_SIGINT;
            sigemptyset(&multiWatch_action.sa_mask);
            multiWatch_action.sa_flags = 0;
            sigaction(SIGINT, &multiWatch_action, NULL);

            executeMultiWatch(pList, output_file);

            // revert back
            sigaction(SIGINT, &action, NULL);
            continue;
        }

        // cout << cmd << endl;

        Pipeline* p = new Pipeline(cmd);
        p->parse();
        if (p->num_active == -1) {
            cout << "Error while parsing command" << endl;
            continue;
        }

        if (p->cmds[0]->args.size() == 0) {
            if (cmd.size() > 0) {
                cout << "Invalid command" << endl;
            }
            continue;
        }

        // cout << *p << endl;
        // continue;

        string arg = p->cmds[0]->args[0];
        if (arg == "cd" || arg == "exit" || arg == "jobs") {
            bool builtin = handleBuiltin(*p);
            if (!builtin) {
                cout << "Error occurred in builtin command" << endl;
            }
            continue;
        }

        p->executePipeline();
    }

    updateHistory();
}

/*
todo: 
ctrl + R
remove tmp files
exception handling
*/