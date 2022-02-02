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
static pid_t fgpid = 0;

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

// https://web.stanford.edu/class/archive/cs/cs110/cs110.1206/lectures/07-races-and-deadlock-slides.pdf
static void reapProcesses(int sig) {
    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0) {
            break;
        }
        int id = ind[pid];
        // if (pid == fgpid) fgpid = 0;  // clear foreground process
        if (WIFSIGNALED(status) || WIFEXITED(status)) {
            all_pipelines[id]->status = DONE;
        } else if (WIFSTOPPED(status)) {
            all_pipelines[id]->status = STOPPED;
        } else if (WIFCONTINUED(status)) {
            all_pipelines[id]->status = RUNNING;
            all_pipelines[id]->num_active = all_pipelines[id]->cmds.size();
        }

        if (all_pipelines[id]->pgid == fgpid && !WIFCONTINUED(status)) {
            all_pipelines[id]->num_active--;
            if (all_pipelines[id]->num_active == 0) {
                fgpid = 0;
            }
        }
    }
}

static void toggleSIGCHLDBlock(int how) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(how, &mask, NULL);
}

void blockSIGCHLD() {
    toggleSIGCHLDBlock(SIG_BLOCK);
}

void unblockSIGCHLD() {
    toggleSIGCHLDBlock(SIG_UNBLOCK);
}

static void waitForForegroundProcess(pid_t pid) {
    fgpid = pid;
    sigset_t empty;

    sigemptyset(&empty);
    while (fgpid == pid) {
        sigsuspend(&empty);
    }
    unblockSIGCHLD();
}

static void CZ_handler(int signum) {
    // cin.setstate(ios::badbit);
    if (signum == SIGINT) {
        ctrlC = 1;
    } else if (signum == SIGTSTP) {
        ctrlZ = 1;
    }
}

void executePipeline(Pipeline& p, bool isMulti = false) {
    // need to set p.pgid to the pid of the first process in the pipeline

    pid_t fg_pgid = 0;
    int new_pipe[2], old_pipe[2];

    blockSIGCHLD();

    int cmds_size = p.cmds.size();
    for (int i = 0; i < cmds_size; i++) {
        if (i + 1 < cmds_size) {
            int ret = pipe(new_pipe);
            if (ret < 0) {
                perror("pipe");
                exit(1);
            }
        }
        pid_t cpid = fork();
        if (cpid < 0) {
            perror("fork");
            exit(1);
        }
        if (cpid == 0) {  // child
            unblockSIGCHLD();
            // revert signal handlers to default behaviour
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            // if (i == 0 || i + 1 == cmds_size) {
            p.cmds[i]->io_redirect();
            // }

            if (i == 0) {
                setpgrp();
            } else {
                setpgid(0, fg_pgid);
                dup2(old_pipe[0], p.cmds[i]->fd_in);  // input piping
                close(old_pipe[0]);
                close(old_pipe[1]);
            }

            if (i < cmds_size) {
                dup2(new_pipe[1], p.cmds[i]->fd_out);  // output piping
                close(new_pipe[0]);
                close(new_pipe[1]);
            }

            if (p.cmds[i]->args[0] == "history") {
                printHistory();
                exit(0);
            } else {
                vector<char*> c_args = cstrArray(p.cmds[i]->args);
                execvp(c_args[0], c_args.data());
                perror("execvp");
                exit(1);
            }

        } else {  // parent shell
            p.cmds[i]->pid = cpid;

            if (i == 0) {
                fg_pgid = cpid;
                p.pgid = cpid;

                // cout << cpid << endl;

                setpgid(cpid, fg_pgid);
                all_pipelines.push_back(&p);

                // cout << p << endl;

                // https://web.archive.org/web/20170701052127/https://www.usna.edu/Users/cs/aviv/classes/ic221/s16/lab/10/lab.html
                tcsetpgrp(STDIN_FILENO, fg_pgid);
            } else {
                setpgid(cpid, fg_pgid);
            }

            if (i > 0) {
                close(old_pipe[0]);
                close(old_pipe[1]);
            }
            old_pipe[0] = new_pipe[0];
            old_pipe[1] = new_pipe[1];

            // extra handling
            ind[cpid] = (int)all_pipelines.size() - 1;
        }
    }

    if (p.cmds.back()->is_bg) {
        unblockSIGCHLD();
    } else {
        waitForForegroundProcess(fg_pgid);
        if (all_pipelines.back()->status == STOPPED) {
            kill(-fg_pgid, SIGCONT);
        }
    }
    tcsetpgrp(STDIN_FILENO, getpid());

    // for(auto it : all_pipelines) {
    //     cout << *it << endl;
    // }
    // cout << *all_pipelines.back() << endl;
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

        // cout << cmd << endl;

        Pipeline* p = getCommand(cmd);
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

        // cout << p << endl;

        // cout << "CHAR ARRAY" << endl;
        // for(int i = 0; i < p.cmds.size(); i++) {
        //     // cout << p.cmds[i]->c_args.size() << endl;
        //     for(int j = 0; j < p.cmds[i]->c_args.size() && p.cmds[i]->c_args[j] != nullptr; j++) {
        //         cout << p.cmds[i]->c_args[j] << " ";
        //     }
        //     cout << endl;
        // }

        string arg = p->cmds[0]->args[0];
        if (arg == "cd" || arg == "exit" || arg == "jobs") {
            bool builtin = handleBuiltin(*p);
            if (!builtin) {
                cout << "Error occurred in builtin command" << endl;
            }
            continue;
        }

        executePipeline(*p);

        // add exit
        // add builtins

        // 1 to last should read from stdin
        // 0 to last - 1 should output to stdout
        // doubtful : ls > temp.txt | sort -r
    }

    updateHistory();
}

/*
jobs
exit
cd (optional)
multiwatch
history
autocomplete
*/