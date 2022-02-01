#include <bits/stdc++.h>
using namespace std;

vector<char*> cstrArray(vector<string>& args) {
    vector<char*> args_(args.size() + 1);
    for (int i = 0; i < args.size(); i++) {
        args_[i] = (char*)malloc((args[i].length() + 1) * sizeof(char));
        strcpy(args_[i], args[i].c_str());
    }
    args_[args.size()] = (char*)malloc(sizeof(char));
    args_[args.size()] = NULL;
    // strcpy(args_[args.size()], "\0");
    return args_;
}

int main() {
    vector<string> args = {"sort", "test.txt"};
    vector<char*> c_args = cstrArray(args);
    for (int i = 0; c_args[i] != NULL; i++) {
        cout << c_args[i] << endl;
    }
}