#include <dirent.h>
#include <sys/types.h>

#include <algorithm>

#include "header.h"

using namespace std;

vector<string> getFilesInCurrDir() {
    vector<string> files;
    DIR* dir = opendir(".");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            DIR* sub_dir = opendir(entry->d_name);
            if (sub_dir) {
                closedir(sub_dir);
            } else {
                string file(entry->d_name);
                files.push_back(file);
            }
        }
        closedir(dir);
    } else {
        cout << "Directory could not be opened";
        // handle case? do better error handling
    }
    return files;
}

vector<string> autocomplete(string s) {
    vector<string> filenames = getFilesInCurrDir();
    vector<string> matched;
    int s_len = s.length();
    for (int i = 0; i < filenames.size(); i++) {
        if (filenames[i].length() < s_len) {
            continue;
        } else if (s == filenames[i].substr(0, s_len)) {
            matched.push_back(filenames[i]);
        }
    }
    if (matched.size() == 1) {
        s = matched[0];
    } else if (matched.size() > 1) {
        sort(matched.begin(), matched.end());
        int i = 0;
        s = "";
        while (i < min(matched[0].size(), matched.back().size()) && matched[0][i] == matched.back()[i]) {
            s += matched[0][i++];
        }
    }
    return matched;
}
