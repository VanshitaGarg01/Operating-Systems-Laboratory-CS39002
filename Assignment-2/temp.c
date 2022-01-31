#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    // perform bootup tasks, like loading history into deque

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
        char* args[] = {"cat", "temp.txt", NULL};
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    }
    // else {
    //     while(1) {
    //         printf("bye\n");
    //     }
    // }
}
