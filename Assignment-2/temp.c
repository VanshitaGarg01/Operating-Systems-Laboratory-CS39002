// #include <signal.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/signal.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <unistd.h>

// #define COLOR_RED "\033[1;31m"
// #define COLOR_GREEN "\033[1;32m"
// #define COLOR_YELLOW "\033[1;33m"
// #define COLOR_BLUE "\033[1;34m"
// #define COLOR_MAGENTA "\033[1;35m"
// #define COLOR_CYAN "\033[1;36m"
// #define COLOR_RESET "\033[0m"

// #define ERROR(msg, ...) printf(COLOR_RED "[ERROR] " msg COLOR_RESET "\n", ##__VA_ARGS__);
// #define SUCCESS(msg, ...) printf(COLOR_CYAN "[INFO] " msg COLOR_RESET "\n", ##__VA_ARGS__);
// #define DEBUG(msg, ...) printf(COLOR_BLUE "[DEBUG] " msg COLOR_RESET "\n", ##__VA_ARGS__);
// #define LOG(msg, ...) printf(COLOR_BLUE "[LOG] " msg COLOR_RESET "\n", ##__VA_ARGS__);
// #define PROMPT(msg, ...) printf(COLOR_GREEN msg COLOR_RESET  "\n", ##__VA_ARGS__);

// int main(int argc, char *argv[]) {
//     int a[10];
//     for(int i = 0; i < 10; i++) {
//         scanf("%d", &a[i]);
//         // a[i] = i;
//     }

//     for(int i = 0; i < 10; i++) {
//         printf("%d\n", a[i]);
//     }
//     while(1);
// }


#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
int main()
{
        //A null terminated array of character 
        //pointers
        char *args[]={NULL};
        execvp(args[0],args);
      
        /*All statements are ignored after execvp() call as this whole 
        process(execDemo.c) is replaced by another process (EXEC.c)
        */
        printf("Ending-----");
      
    return 0;
}