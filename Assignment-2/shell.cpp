#include <signal.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <vector>

#include "Command.h"
#include "read_command.h"
#include "utility.h"

using namespace std;

vector<Command*> cmds;

int main() {
    // perform bootup tasks, like loading history into deque

    pid_t shid = getpid();

    // add signal handlers
    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    // sigaction(SIGINT, &action, NULL);
    // sigaction(SIGTSTP, &action, NULL);

    while (!cin.eof()) {
        cmds = getCommand();
        if (cmds.size() == 0) {
            continue;
        }

        // 1 to last should read from stdin
        // 0 to last - 1 should output to stdout
        // doubtful : ls > temp.txt | sort -r

        int num_cmds = cmds.size();
        for (int i = 0; i < num_cmds; i++) {
            pid_t pid = fork();
            if(pid == 0) {
                
            }
        }
    }
}
