#include "signal_handlers.h"

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <map>

using namespace std;

// https://web.stanford.edu/class/archive/cs/cs110/cs110.1206/lectures/07-races-and-deadlock-slides.pdf
void reapProcesses(int signum) {
    while (true) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (pid <= 0) {
            break;
        }

        int id = ind[pid];
        Pipeline* pipeline = all_pipelines[id];
        if (WIFSIGNALED(status) || WIFEXITED(status)) {
            pipeline->num_active--;
            if (pipeline->num_active == 0) {
                pipeline->status = DONE;
                // remove from watch list
                if (pgid_wd.find(pipeline->pgid) != pgid_wd.end()) {
                    inotify_rm_watch(inotify_fd, pgid_wd[pipeline->pgid]);
                }
            }
        } else if (WIFSTOPPED(status)) {
            pipeline->num_active--;
            if (pipeline->num_active == 0) {
                pipeline->status = STOPPED;
            }
        } else if (WIFCONTINUED(status)) {
            pipeline->num_active++;
            if (pipeline->num_active == (int)pipeline->cmds.size()) {
                pipeline->status = RUNNING;
            }
        }

        if (pipeline->pgid == fgpid && !WIFCONTINUED(status)) {
            if (pipeline->num_active == 0) {
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
    if (signum == SIGINT) {
        ctrlC = 1;
    } else if (signum == SIGTSTP) {
        ctrlZ = 1;
    }
}

void multiWatch_SIGINT(int signum) {
    for (auto it = pgid_wd.begin(); it != pgid_wd.end(); it++) {
        kill(-it->first, SIGINT);
    }
    close(inotify_fd);
}