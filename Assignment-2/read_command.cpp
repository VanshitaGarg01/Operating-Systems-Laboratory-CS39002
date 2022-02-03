#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>

#include "header.h"
using namespace std;

extern bool ctrlC, ctrlZ, ctrlD;

void displayPrompt() {
    char buf[DIR_LENGTH];
    getcwd(buf, DIR_LENGTH);
    string dir(buf);
    dir = dir.substr(dir.find_last_of("/") + 1);
    cout << COLOR_GREEN << "vash: " << COLOR_BLUE << dir << COLOR_RESET << "$ ";
}

int handleChar(char c, string& buf) {
    if (c == CTRL_CZ) {  // ctrl + C, ctrl + Z
        if (ctrlC) {
            printf("\n");
            buf = "";
            ctrlC = 0;
            return 2;
        } else if (ctrlZ) {
            ctrlZ = 0;
            return 0;
        }
    } else if (c == CTRL_D) {  // ctrl + D
        ctrlD = 1;
        buf = "";
        return 4;
    } else if (c == BACKSPACE) {  // backspace
        if (buf.length() > 0) {
            printf("\b \b");
            buf.pop_back();
        }
        return 0;
    } else if (c == ENTER) {  // enter
        printf("\n");
        return 1;
    } else {
        printf("%c", c);
        buf += c;
        return 0;
    }
    return 0;
}

string readCommand() {
    struct termios old_tio, new_tio;
    signed char c;

    /* get the terminal settings for stdin */
    tcgetattr(STDIN_FILENO, &old_tio);

    /* we want to keep the old setting to restore them a the end */
    new_tio = old_tio;

    /* disable canonical mode (bufered i/o) and local echo */
    new_tio.c_lflag &= (~ICANON & ~ECHO);
    new_tio.c_cc[VMIN] = 1;
    new_tio.c_cc[VTIME] = 0;
    /* set the new settings immediately */
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
    string buf;
    while (1) {
        int ret_in, ret_out;
        c = getchar();
        if (c == CTRL_R) {  // ctrl + r
            printf("\n");
            printf("Enter search term: "); // should i print buf here?
            string s; // = buf;
            while (1) {
                char ch = getchar();
                ret_in = handleChar(ch, s);
                if (ret_in != 0) {
                    break;
                }
            }
            if (ret_in == 4) {  // ctrl + D
                ctrlD = 0;
                buf = "";
                break;
            } else if (ret_in == 2) {  // ctrl + C
                buf = "";
                break;
            }

            vector<string> cmds = searchInHistory(s);
            if (cmds.size() == 0) {
                printf("No match for search term in history\n");
                displayPrompt();
                // should s reappear in the prompt?
            } else if (cmds.size() == 1) {
                printf("Exact match found\n");
                displayPrompt();
                buf = cmds[0];
                printf("%s", buf.c_str());
            } else {
                printf("Similar commands matched (most recent first):\n");
                for (int i = 0; i < (int)cmds.size(); i++) {
                    printf("%s\n", cmds[i].c_str());
                }
                displayPrompt();
                // should i display buf?
            }

        } else if (c == TAB) {  // tab
            string s;
            for (int j = buf.length() - 1; j >= 0 && buf[j] != ' ' && buf[j] != '/'; j--) {
                s += buf[j];
            }
            reverse(s.begin(), s.end());
            vector<string> complete_options = autocomplete(s);
            if (complete_options.size() == 0) {
                continue;
            } else if (complete_options.size() == 1) {
                string str = complete_options[0].substr(s.length());
                buf += str;
                printf("%s", str.c_str());
            } else {
                printf("\n");
                for (int j = 0; j < (int)complete_options.size(); j++) {
                    printf("%d. %s ", j + 1, complete_options[j].c_str());
                }
                printf("\n");
                printf("Enter choice: ");
                string n;
                while (1) {
                    char ch = getchar();
                    ret_in = handleChar(ch, n);
                    if (ret_in != 0) {
                        break;
                    }
                }
                if (ret_in == 4) {  // ctrl + D
                    ctrlD = 0;
                    buf = "";
                    break;
                } else if (ret_in == 2) {  // ctrl + C
                    buf = "";
                    break;
                }
                try {
                    int num = stoi(n);
                    if (num > (int)complete_options.size() || num <= 0) {
                        printf("Invalid Choice\n");
                    } else {
                        buf += complete_options[num - 1].substr(s.length());
                    }
                } catch (...) {
                    printf("Invalid Choice\n");
                }
                displayPrompt();
                printf("%s", buf.c_str());
            }
        } else {
            ret_out = handleChar(c, buf);
            if (ret_out != 0) {
                break;
            }
        }
    }

    /* restore the former settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    return buf;
}
