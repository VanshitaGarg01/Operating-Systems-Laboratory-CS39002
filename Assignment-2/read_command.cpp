#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>

#include "header.h"
using namespace std;

void displayPrompt() {
    char buf[DIR_LENGTH];
    getcwd(buf, DIR_LENGTH);
    string dir(buf);
    dir = dir.substr(dir.find_last_of("/") + 1);
    cout << COLOR_GREEN << "vash: " << COLOR_BLUE << dir << COLOR_RESET << "$ ";
}

// string readCommand() {
//     string command;
//     getline(cin, command);
//     if (cin.bad()) {
//         cin.clear();
//         cout << endl;
//         return "";
//     }
//     // cout << command << " " << command.length() << endl;
//     return command;
//     // getline();
//     // currently may just read string using getline, however we will shift to termios, char by char
//     // return the command as it is as read
// }

string readCommand() {
    struct termios old_tio, new_tio;
    unsigned char c;

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
    do {
        c = getchar();
        if (c == 18) {  // ctrl + r
            // putchar('a');
            // later ?
        } else if (c == 127) {  // '\b'
            if (buf.length() > 0) {
                printf("\b \b");
                buf.pop_back();
            }
        } else if (c == '\t') {  // tab
            string s;
            for (int j = buf.length() - 1; j >= 0 && buf[j] != ' '; j--) {
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
                for (int j = 0; j < complete_options.size(); j++) {
                    printf("%d. %s ", j + 1, complete_options[j].c_str());
                }
                printf("\n");
                printf("Enter choice: ");
                string n;
                while (1) {
                    char ch = getchar();
                    if (ch == '\n') {
                        printf("\n");
                        break;
                    } else if (ch == 127) {
                        if (n.length() > 0) {
                            printf("\b \b");
                            n.pop_back();
                        }
                    } else {
                        putchar(ch);
                        n += ch;
                    }
                }
                int num = stoi(n);
                if (num > complete_options.size() || num <= 0) {
                    printf("Invalid Choice\n");
                } else {
                    buf += complete_options[num - 1].substr(s.length());
                }
                // PROMPT();
                printf("Assignment-2>> ");
                printf("%s", buf.c_str());
            }
        } else if (c == 10) {  // newline
            printf("\n");
            break;  // ?
        } else {
            buf += c;
            printf("%c", c);
        }
    } while (1);

    /* restore the former settings */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
    // string command(buf);
    // addToHistory(command);
    return buf;
}

Pipeline* getCommand(string cmd) {
    // cout << "Assignment-2>> ";
    vector<string> piped_cmds = split(cmd, '|');
    // cout << piped_cmds.size() << endl;
    vector<Command*> cmds;

    bool flag = 1;
    for (auto cmd_str : piped_cmds) {
        Command* c = new Command(cmd_str);
        if (c->parse()) {
            cmds.push_back(c);
        } else {
            flag = 0;
            break;
        }
    }
    if (flag) {
        return new Pipeline(cmds);
    } else {
        return new Pipeline(-1);
    }
}
