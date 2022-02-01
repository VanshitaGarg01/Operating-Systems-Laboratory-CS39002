#include <stdio.h>
#include <termios.h>
#include <unistd.h>

int main() {
    struct termios old_tio, new_tio;
    unsigned char c;

    /* get the terminal settings for stdin */
    tcgetattr(STDIN_FILENO, &old_tio);

    /* we want to keep the old setting to restore them a the end */
    new_tio = old_tio;

    /* disable canonical mode (buffered i/o) and local echo */
    new_tio.c_lflag &= (~ICANON & ~ECHO);
    new_tio.c_cc[VMIN] = 1;
    new_tio.c_cc[VTIME] = 0;
    /* set the new settings immediately */
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    int i = 0;
    char buff[10000];
    do {
        c = getchar();
        // printf("%d\n", c);
        if(c == 18) {   // ctrl + r
            putchar('a');
        }
        if (c == 127) {
            printf("\b \b");
            i--;
        } else {
            buff[i++] = c;
            printf("%c", c);
        }
    } while (c != '\t' && c != '\n');
    // if (c == '1')
    buff[i] = '\0';
    printf("%s", buff);

    /* restore the former settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

    return 0;
}

/*
implement autocomplete algo
if ctrl-r detected redirect to diff function (can ignore)
if tab detected, call autocomplete function with part of input since last whitespace
return a vector<string> v, if sz(v) == 1, print on screen and buffer and adjust i accordingly
if multiple options, display menu, take option input, print on screen and buffer and adjust i accordingly

vash>> abc
1. abcd.txt 2. abc.txt
1
vash>> abc

*/