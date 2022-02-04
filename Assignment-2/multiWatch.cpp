#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>

#include "header.h"
using namespace std;

int inotify_fd;
map<pid_t, int> pgid_wd;
map<int, int> wd_ind;

vector<Pipeline*> parseMultiWatch(string cmd, string& output_file) {
    trim(cmd);
    if (cmd.length() < 10) {  // not multiwatch
        return vector<Pipeline*>();
    }
    vector<Pipeline*> pipelines;
    if (cmd.substr(0, 10) == "multiWatch") {
        cmd = cmd.substr(10);
        trim(cmd);
        // DEBUG("%s", cmd.c_str());
        if (cmd.length()) {
            if (cmd.back() != ']') {
                int i = cmd.length() - 1;
                while (i >= 0 && cmd[i] != ']') {
                    i--;
                }
                if (i == -1) {
                    throw ShellException("Could not find ']");
                } else {
                    output_file = cmd.substr(i + 1);
                    cmd = cmd.substr(0, i + 1);
                    trim(output_file);
                    if (output_file.length()) {
                        if (output_file[0] == '>') {
                            output_file = output_file.substr(1);
                            trim(output_file);
                            if (output_file.length() == 0) {
                                throw ShellException("No output file after >");
                            }
                        } else {
                            throw ShellException("Unable to parse after multiWatch");
                        }
                    }
                }
            }
            trim(cmd);
            cmd.pop_back();
            if (cmd[0] == '[') {
                cmd = cmd.substr(1);
                vector<string> commands = split(cmd, ',');
                for (auto& command : commands) {
                    trim(command);
                    if (command[0] == '\"' && command.back() == '\"') {
                        command = command.substr(1, command.length() - 2);
                    } else {
                        throw ShellException("No quotes around command");
                    }
                }
                for (auto& command : commands) {
                    Pipeline* p = new Pipeline(command);
                    p->parse();
                    pipelines.push_back(p);
                }
            } else {
                throw ShellException("Could not find '[");
            }
        } else {
            throw ShellException("No commands after multiWatch");
        }
    } else {
        return vector<Pipeline*>();
    }
    return pipelines;
}

void executeMultiWatch(vector<Pipeline*>& pList, string output_file) {
    inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        perror("inotify_init");
        // exit(1);
        throw ShellException("Unable to create inotify instance");
    }

    vector<int> fds;
    for (int i = 0; i < (int)pList.size(); i++) {
        pList[i]->executePipeline(true);
        string tmpFile = ".tmp" + to_string(pList[i]->pgid) + ".txt";
        int fd = open(tmpFile.c_str(), O_RDONLY);
        if (fd < 0) {
            int temp_fd = open(tmpFile.c_str(), O_CREAT, 0644);
            if (temp_fd < 0) {
                perror("open");
                // exit(1);
                throw ShellException("Unable to open tmp file");
            }
            close(temp_fd);
            fd = open(tmpFile.c_str(), O_RDONLY);
        }
        fds.push_back(fd);
        int wd = inotify_add_watch(inotify_fd, tmpFile.c_str(), IN_MODIFY);
        if (wd < 0) {
            perror("inotify_add_watch");
            // exit(1);
            throw ShellException("Unable to add watch");
        }
        pgid_wd[pList[i]->pgid] = wd;
        wd_ind[wd] = i;
    }

    // DEBUG("pgid: %d", pList[0]->pgid);
    // DEBUG("fds[0]: %d", fds[0]);
    // DEBUG("wd: %d", pgid_wd[pList[0]->pgid]);

    int num_running = pList.size();

    // DEBUG("output_file: %s", output_file.c_str());
    FILE* out_fp;
    if (output_file == "") {
        out_fp = stdout;
    } else {
        out_fp = fopen(output_file.c_str(), "w");
        if (out_fp == NULL) {
            perror("fopen");
            // exit(1);
            throw ShellException("Unable to open output file");
        }
        // DEBUG("out_fp: %p", out_fp);
    }

    int event_size = sizeof(struct inotify_event);
    int event_buf_len = 1024 * (event_size + 16);
    struct inotify_event event_buf[event_buf_len];

    char buf[BUF_SIZE];

    while (num_running) {
        // DEBUG("here");
        int num_read = read(inotify_fd, event_buf, sizeof(event_buf));
        // DEBUG("num_read: %d", num_read);
        if (num_read < 0 && errno != EINTR && errno != EBADF) {
            // DEBUG("errno: %d", errno);
            perror("read");
            // exit(1);
            throw ShellException("Unable to read from inotify instance");
        } else if (errno == EBADF) {
            break;
        }

        int i = 0;
        while (i < num_read) {
            // DEBUG("trying");
            struct inotify_event* event = (struct inotify_event*)&event_buf[i];
            // DEBUG("mask: %d", event->mask);
            if ((event->mask & IN_MODIFY) || (event->mask & IN_IGNORED)) {
                // DEBUG("event->mask IN_MODIFY: %d", event->mask & IN_MODIFY);
                // DEBUG("event->mask IN_IGNORED: %d", event->mask & IN_IGNORED);
                int fd = fds[wd_ind[event->wd]];
                int len, seen_data = 0;
                while ((len = read(fd, buf, BUF_SIZE)) > 0 || errno == EINTR) {
                    if (!seen_data) {
                        int sz = pList[wd_ind[event->wd]]->cmd.length() + to_string(time(NULL)).length() + 4;
                        string line(sz, '-');
                        fprintf(out_fp, "%s\n%s, %ld :\n%s\n", line.c_str(), pList[wd_ind[event->wd]]->cmd.c_str(), time(NULL), line.c_str());
                        fflush(out_fp);
                        seen_data = 1;
                    }
                    buf[len] = '\0';
                    // DEBUG("%s", buf);
                    fprintf(out_fp, "%s", buf);
                    fflush(out_fp);
                    // DEBUG("x: %d", x);
                }
            }
            if (event->mask & IN_IGNORED) {
                num_running--;
                close(fds[wd_ind[event->wd]]);
                // DEBUG("pgid: %d", pList[wd_ind[event->wd]]->pgid);
                // DEBUG("num_running: %d", num_running);
            }

            i += event_size + event->len;
            // DEBUG("there");
        }
        // DEBUG("num_running: %d", num_running);
    }

    for (auto it = pgid_wd.begin(); it != pgid_wd.end(); it++) {
        string tmpFile = ".tmp" + to_string(it->first) + ".txt";
        if (remove(tmpFile.c_str()) < 0) {
            perror("remove");
            // exit(1);
            throw ShellException("Unable to remove tmp file");
        }
    }
    pgid_wd.clear();
    // DEBUG("yahan");
    close(inotify_fd);
    // DEBUG("wahan");
    if (out_fp != stdout) {
        fclose(out_fp);
    }
}