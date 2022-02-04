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

void shellCd(string arg) {
    trim(arg);
    if (arg == "") {
        throw ShellException("No directory specified");
    }
    if (chdir(arg.c_str()) < 0) {
        perror("chdir");
    }
}

void shellExit(string arg = "") {
    updateHistory();
    exit(0);
}

void shellJobs(string arg = "") {
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
}

vector<string> builtins = {"cd", "exit", "jobs"};
void (*builtin_funcs[])(string args) = {&shellCd, &shellExit, &shellJobs};

void handleBuiltin(Pipeline& p) {
    try {
        string cmd = p.cmds[0]->args[0];
        for (int i = 0; i < (int)builtins.size(); i++) {
            if (cmd == builtins[i]) {
                string arg = (p.cmds[0]->args.size() > 1) ? p.cmds[0]->args[1] : "";
                (*builtin_funcs[i])(arg);
            }
        }
    } catch (ShellException& e) {
        throw;
    }
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

    while (!ctrlD) {
        displayPrompt();
        string cmd = readCommand();
        trim(cmd);
        if (cmd == "") {
            continue;
        }
        addToHistory(cmd);

        try {
            string output_file = "";
            vector<Pipeline*> pList = parseMultiWatch(cmd, output_file);
            if (pList.size() > 0) {  // multiWatch
                struct sigaction multiWatch_action;
                multiWatch_action.sa_handler = multiWatch_SIGINT;
                sigemptyset(&multiWatch_action.sa_mask);
                multiWatch_action.sa_flags = 0;
                sigaction(SIGINT, &multiWatch_action, NULL);
                executeMultiWatch(pList, output_file);
                // revert back
                sigaction(SIGINT, &action, NULL);
            } else {
                Pipeline* p = new Pipeline(cmd);
                p->parse();
                string arg = p->cmds[0]->args[0];
                if (arg == "cd" || arg == "exit" || arg == "jobs") {
                    handleBuiltin(*p);
                    continue;
                }
                // cout << *p << endl;
                // DEBUG("parsed");
                p->executePipeline();
                // DEBUG("executed");
            }
        } catch (ShellException& e) {
            cout << e.what() << endl;
        }
    }

    updateHistory();
}
