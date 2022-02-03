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
    }
    return files;
}

vector<string> autocomplete(string s) {
    vector<string> filenames = getFilesInCurrDir();
    vector<string> matched;
    int s_len = s.length();
    for (int i = 0; i < (int)filenames.size(); i++) {
        if ((int)filenames[i].length() < s_len) {
            continue;
        } else if (s == filenames[i].substr(0, s_len)) {
            matched.push_back(filenames[i]);
        }
    }

    if ((int)matched.size() <= 1) {
        return matched;
    } else {
        sort(matched.begin(), matched.end());
        int i = 0;
        string suggest = "";
        while (i < (int)min(matched.front().size(), matched.back().size()) && matched.front()[i] == matched.back()[i]) {
            suggest += matched[0][i++];
        }
        if (suggest.length() > s.length()) {
            return vector<string>(1, suggest);
        } else {
            return matched;
        }
    }
}
