#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <map>

#include "header.h"
using namespace std;

extern bool ctrlC, ctrlZ, ctrlD;
extern pid_t fgpid;

extern vector<Pipeline*> all_pipelines;
extern map<pid_t, int> ind;

// https://web.stanford.edu/class/archive/cs/cs110/cs110.1206/lectures/07-races-and-deadlock-slides.pdf
void reapProcesses(int sig) {
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

void toggleSIGCHLDBlock(int how) {
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

void waitForForegroundProcess(pid_t pid) {
    fgpid = pid;
    sigset_t empty;

    sigemptyset(&empty);
    while (fgpid == pid) {
        sigsuspend(&empty);
    }
    unblockSIGCHLD();
}

void CZ_handler(int signum) {
    // cin.setstate(ios::badbit);
    if (signum == SIGINT) {
        ctrlC = 1;
    } else if (signum == SIGTSTP) {
        ctrlZ = 1;
    }
}