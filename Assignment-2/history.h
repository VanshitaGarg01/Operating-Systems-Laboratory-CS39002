#ifndef __HISTORY_H
#define __HISTORY_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

using namespace std;

deque<string> history;

void loadHistory () {
    history.clear();
    ifstream file(".shell_history");
    if (!file.is_open()) {
        //  No shell histrory?
        cout << "No file to open\n";
        return;
    } else {
        string line = "";
        while (getline(file, line)) {
            history.push_back(line);
        }
    }
    file.close();
    return;
}

string searchInHistory (string s) {
    string command = "";
    for (auto it: history) {
        char ch = '\0' + 229;
        string t = s + ch + it;
        int n = t.size();
        vector<int> lps(n + 1);
        int i = 0, j = -1;
        lps[0] = -1;
        while(i < n) {
            while(j != -1 && t[j] != t[i]) {
                j = lps[j];
            }
            i++; j++;
            lps[i] = j;
            if (lps[i] == s.size()) {
                if (s.size() == it.size()) {
                    return s;
                } else if (command == "") {
                    command = it;
                }
            }
        }

    }
    return (s.size() > 1 ? command : "");
}

void printHistory () {
    for (int i = min(1000, (int)history.size()) - 1; i >= 0 ; i--) {
        cout << history[i] << endl;
    }
    return;
}

void addToHistory (string s) {
    if (history.size() == 10000) {
        history.pop_back();
    }
    history.push_front(s);
    return;
}

void updateHistory () {
    // int fd = open(".shell_history.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR); 
    ofstream file(".shell_history");
    if (!file.is_open()) {
        //  No shell histrory?
        return;
    } else {
        for (auto it: history) {
            string temp = it+"\n";
            file << temp;
        }
    }
    file.close();
    return;
}

#endif

