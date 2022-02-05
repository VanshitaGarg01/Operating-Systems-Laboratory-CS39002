#include <deque>
#include <fstream>

#include "header.h"

using namespace std;

extern deque<string> history;

void loadHistory() {
    history.clear();
    ifstream file(".shell_history");
    if (!file.is_open()) {
        return;
    } else {
        string line = "";
        while (getline(file, line)) {
            history.push_back(line);
        }
    }
    file.close();
}

vector<string> searchInHistory(string s) {
    vector<string> commands;
    for (int ind = history.size() - 1; ind >= 0; ind--) {
        string hist_cmd = history[ind];
        char ch = '\0' + 229;
        string t = s + ch + hist_cmd;
        int n = t.size();
        vector<int> lps(n + 1);
        int i = 0, j = -1;
        lps[0] = -1;
        while (i < n) {
            while (j != -1 && t[j] != t[i]) {
                j = lps[j];
            }
            i++;
            j++;
            lps[i] = j;
            if (lps[i] == (int)s.size()) {
                if (s.size() == hist_cmd.size()) {  // exact match
                    return vector<string>(1, hist_cmd);
                } else {
                    commands.push_back(hist_cmd);
                }
            }
        }
    }
    return (s.size() > 2 ? commands : vector<string>());
}

void printHistory() {
    int i = max(0, (int)history.size() - HIST_DISPLAY_SIZE);
    for (int cnt = 0; cnt < min((int)history.size(), HIST_DISPLAY_SIZE); i++, cnt++) {
        cout << i + 1 << " " << history[i] << endl;
    }
}

void addToHistory(string s) {
    if (history.size() == HIST_SIZE) {
        history.pop_front();
    }
    history.push_back(s);
}

void updateHistory() {
    // int fd = open(".shell_history.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    ofstream file(HIST_FILE);
    if (!file.is_open()) {
        //  No shell history?
        return;
    } else {
        for (auto it : history) {
            string temp = it + "\n";
            file << temp;
        }
    }
    file.close();
}
