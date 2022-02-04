#include <signal.h>
#include <sys/signal.h>
#include <unistd.h>

#include <map>

#include "header.h"

using namespace std;

extern vector<Pipeline*> all_pipelines;
extern map<pid_t, int> ind;

Pipeline::Pipeline(string& cmd) : cmd(cmd), pgid(-1) {}

Pipeline::Pipeline(vector<Command*>& cmds) : cmds(cmds), pgid(-1), num_active(cmds.size()), status(RUNNING) {}

void Pipeline::parse() {
    trim(this->cmd);
    if (this->cmd.back() == '&') {
        this->is_bg = true;
        this->cmd.pop_back();
    }
    vector<string> piped_cmds = split(this->cmd, '|');

    try {
        vector<Command*> cmds;
        for (int i = 0; i < (int)piped_cmds.size(); i++) {
            Command* c = new Command(piped_cmds[i]);
            c->parse();
            if (c->args.size() == 0) {
                throw ShellException("Empty command");
            }
            cmds.push_back(c);
        }
        this->cmds = cmds;
        this->num_active = cmds.size();
        this->status = RUNNING;
    } catch (ShellException& e) {
        throw;
    }
}

void Pipeline::executePipeline(bool isMultiwatch) {
    pid_t fg_pgid = 0;
    int new_pipe[2], old_pipe[2];

    blockSIGCHLD();

    int cmds_size = this->cmds.size();
    for (int i = 0; i < cmds_size; i++) {
        if (i + 1 < cmds_size) {
            int ret = pipe(new_pipe);
            if (ret < 0) {
                perror("pipe");
                // exit(1);
                throw ShellException("Unable to create pipe");
            }
        }
        pid_t cpid = fork();
        if (cpid < 0) {
            perror("fork");
            // exit(1);
            throw ShellException("Unable to fork");
        }
        if (cpid == 0) {  // child
            unblockSIGCHLD();
            // revert signal handlers to default behaviour
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            if (i == 0) {
                fg_pgid = getpid();
            }
            if (isMultiwatch && i + 1 == cmds_size) {
                this->cmds[i]->output_file = ".tmp" + to_string(fg_pgid) + ".txt";
            }
            this->cmds[i]->io_redirect();

            if (i == 0) {
                setpgrp();
            } else {
                setpgid(0, fg_pgid);
                dup2(old_pipe[0], this->cmds[i]->fd_in);  // input piping
                close(old_pipe[0]);
                close(old_pipe[1]);
            }
            if (i + 1 < cmds_size) {
                dup2(new_pipe[1], this->cmds[i]->fd_out);  // output piping
                close(new_pipe[0]);
                close(new_pipe[1]);
            }

            if (this->cmds[i]->args[0] == "history") {
                printHistory();
                exit(0);
            } else {
                vector<char*> c_args = cstrArray(this->cmds[i]->args);
                execvp(c_args[0], c_args.data());
                perror("execvp");
                exit(1);
            }

        } else {  // parent shell
            this->cmds[i]->pid = cpid;
            if (i == 0) {
                fg_pgid = cpid;
                this->pgid = cpid;
                setpgid(cpid, fg_pgid);
                all_pipelines.push_back(this);

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

            ind[cpid] = (int)all_pipelines.size() - 1;
        }
    }

    if (this->is_bg || isMultiwatch) {
        unblockSIGCHLD();
    } else {
        waitForForegroundProcess(fg_pgid);
        if (all_pipelines.back()->status == STOPPED) {
            kill(-fg_pgid, SIGCONT);
        }
    }
    tcsetpgrp(STDIN_FILENO, getpid());
}

ostream& operator<<(ostream& os, const Pipeline& p) {
    cout << p.pgid << endl;
    for (auto it : p.cmds) {
        os << *it << endl;
    }
    return os;
}
