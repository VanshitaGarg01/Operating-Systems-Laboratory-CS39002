#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    // add signal handlers
    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    sigaction(SIGINT, &action, NULL);
    // sigaction(SIGTSTP, &action, NULL);

    // while (!cin.eof()) {

    // }

    // char* args[] = {"echo", NULL};
    if (fork() == 0) {
        char* args[] = {"./a.out", NULL};
        // setpgrp();
        signal(SIGINT, SIG_DFL);
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else {
        while (1) {
            printf("hi\n");
        }
    }

    int i, in_fd = 0;
    int pipeError = 0;
    int FD[2];  //store the read and write file descripters
    for (i = 0; i < pipeProcesses - 1; i++) {
        pipe(FD);
        args = splitCommand(pipeCommands[i], &noOfTokens);
        status = shellExecute(args, noOfTokens, in_fd, FD[1]);
        close(FD[1]);
        in_fd = FD[0];
    }
    args = splitCommand(pipeCommands[i], &noOfTokens);
    status = shellExecute(args, noOfTokens, in_fd, 1);
}
