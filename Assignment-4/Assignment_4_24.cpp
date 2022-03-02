#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

#define ERROR(msg, ...) printf("\033[1;31m[ERROR] " msg " \033[0m\n", ##__VA_ARGS__);
#define SUCCESS(msg, ...) printf("\033[1;36m[SUCCESS] " msg " \033[0m\n", ##__VA_ARGS__);
#define INFO(msg, ...) printf("\033[1;32m[INFO] " msg " \033[0m\n", ##__VA_ARGS__);
#define DEBUG(msg, ...) printf("\033[1;34m[DEBUG] " msg " \033[0m\n", ##__VA_ARGS__);
#define PROMPT(msg, ...) printf("\033[1;32m" msg "\033[0m", ##__VA_ARGS__);

#define LOCK(mutex_p)                                                            \
    do {                                                                         \
        int ret = pthread_mutex_lock(mutex_p);                                   \
        if (ret != 0) {                                                          \
            ERROR("%d: pthread_mutex_lock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

#define UNLOCK(mutex_p)                                                            \
    do {                                                                           \
        int ret = pthread_mutex_unlock(mutex_p);                                   \
        if (ret != 0) {                                                            \
            ERROR("%d: pthread_mutex_unlock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                               \
        }                                                                          \
    } while (0)

const int MIN_NODES = 100;
const int MAX_NODES = 200;
const int MAX_COMP_TIME = 250;
const int MAX_CHILD_JOBS = 10;
const int MAX_ID = (int)1e8;
const int MIN_PROD_TIME = 10;
const int MAX_PROD_TIME = 20;
const int MAX_PROD_SLEEP = 5;

enum JobStatus {
    WAITING,
    ONGOING,
    DONE
};

struct Node {
    int job_id;
    int comp_time;  // in milliseconds
    int child_jobs[MAX_CHILD_JOBS];
    int child_count;
    int parent;
    JobStatus status;
    pthread_mutex_t mutex;

    Node() {
        job_id = rand() % MAX_ID + 1;
        comp_time = rand() % MAX_COMP_TIME + 1;
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            child_jobs[i] = -1;
        }
        child_count = 0;
        parent = -1;
        status = WAITING;
    }

    Node &operator=(const Node &node) {
        job_id = node.job_id;
        comp_time = node.comp_time;
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            child_jobs[i] = node.child_jobs[i];
        }
        child_count = node.child_count;
        parent = node.parent;
        status = node.status;
        return *this;
    }

    int addChild(int child_id) {
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            if (child_jobs[i] == -1) {
                child_jobs[i] = child_id;
                child_count++;
                return i;
            }
        }
        return -1;
    }

    void setParent(int parent_id) {
        parent = parent_id;
    }

    int deleteChild(int child_id) {
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            if (child_jobs[i] == child_id) {
                child_jobs[i] = -1;
                child_count--;
                return i;
            }
        }
        return -1;
    }
};

struct SharedMem {
    Node tree[MAX_NODES];
    int node_count;
    int root;
    pthread_mutex_t mutex;

    void init() {
        node_count = 0;
        root = -1;
        for (int i = 0; i < MAX_NODES; i++) {
            tree[i].status = DONE;
            pthread_mutexattr_t attr;
            pthread_mutexattr_init(&attr);
            pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
            pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
            pthread_mutex_init(&tree[i].mutex, &attr);
        }
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
    }

    int addNode(Node &node, bool locked = false) {
        if (node_count == MAX_NODES) {
            // ERROR("node_count == MAX_NODES");
            return -1;
        }
        for (int i = 0; i < MAX_NODES; i++) {
            if (tree[i].status == DONE) {
                LOCK(&tree[i].mutex);
                tree[i] = node;
                UNLOCK(&tree[i].mutex);
                if (locked) {
                    // INFO("Trying to acquire lock for node %d", i);
                    LOCK(&tree[i].mutex);
                    // INFO("Acquired lock for node %d", i);
                }
                node_count++;
                return i;
            }
        }
        return -1;
    }
};

SharedMem *shm;
int shmid;

int rand(int low, int high) {
    return low + rand() % (high - low + 1);
}

void genInitialTree() {
    int n = rand(MIN_NODES, MAX_NODES);
    for (int i = 0; i < n; i++) {
        Node node;
        int node_pos = shm->addNode(node);
        if (node_pos != -1) {
            if (shm->root == -1) {
                shm->root = node_pos;
                assert(shm->root == 0);
            } else {
                int par = rand(0, node_pos - 1);
                int status = shm->tree[par].addChild(node_pos);
                if (status != -1) {
                    shm->tree[node_pos].setParent(par);
                } else {
                    cout << "Error: Failed to add child to parent" << endl;
                }
            }
        } else {
            cout << "Error: Failed to add node to tree" << endl;
        }
    }
}

void sigintHandler(int sig) {
    if (sig == SIGSEGV) {
        cout << "Segmentation fault" << endl;
    } else if (sig == SIGINT) {
        cout << "Interrupt signal" << endl;
    } else {
        cout << "Unknown signal" << endl;
    }
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    exit(1);
}

int getRandomJob() {
    for (int i = 0; i < MAX_NODES; i++) {
        // TODO: may change choice logic
        // double prob = (double)rand() / (double)RAND_MAX;
        double prob = 0.5;
        double x = (double)rand() / (double)RAND_MAX;
        LOCK(&shm->tree[i].mutex);
        if (shm->tree[i].status == WAITING && x < prob && shm->tree[i].child_count < MAX_CHILD_JOBS) {
            // DEBUG("Random job found: %d", i);
            return i;
        }
        UNLOCK(&shm->tree[i].mutex);
    }
    return -1;
}

void *producer(void *arg) {
    int ind = *(int *)arg;
    int runtime = rand(MIN_PROD_TIME, MAX_PROD_TIME);
    DEBUG("Producer %d: %d", ind, runtime);
    auto start = chrono::high_resolution_clock::now();
    while (1) {
        auto curr = chrono::high_resolution_clock::now();
        auto diff = chrono::duration_cast<chrono::seconds>(curr - start);
        if (diff.count() >= runtime) {
            INFO("Producer %d finished", ind);
            pthread_exit(NULL);
        }

        int par_pos = getRandomJob();
        // INFO("Producer found random job: %d", par_pos);
        if (par_pos == -1) {
            continue;
        }
        Node node;
        LOCK(&shm->mutex);
        int child_pos = shm->addNode(node, true);
        UNLOCK(&shm->mutex);
        if (child_pos == -1) {
            // ERROR("%d, Failed to add node to tree", __LINE__);
            UNLOCK(&shm->tree[par_pos].mutex);
            // sleep(1);
            continue;
        }
        int status = shm->tree[par_pos].addChild(child_pos);
        if (status != -1) {
            shm->tree[child_pos].setParent(par_pos);
            INFO("Producer %d added child %d to parent %d", ind, child_pos, par_pos);
        } else {
            // cout << "Error: Failed to add child to parent" << endl;
            // ERROR("Producer %d failed to add child %d to parent %d", ind, child_pos, par_pos);
        }
        UNLOCK(&shm->tree[child_pos].mutex);
        UNLOCK(&shm->tree[par_pos].mutex);

        int sleep_time = rand(0, MAX_PROD_SLEEP);
        usleep(sleep_time * 1000);
    }
}

int getLeaf(int start) {
    // for (int i = 0; i < MAX_NODES; i++) {
    //     LOCK(&shm->tree[i].mutex);
    //     if (shm->tree[i].child_count == 0) {
    //         UNLOCK(&shm->tree[i].mutex);
    //         return i;
    //     }
    //     UNLOCK(&shm->tree[i].mutex);
    // }
    // return -1;
    LOCK(&shm->tree[start].mutex);
    if (shm->tree[start].child_count == 0 && shm->tree[start].status == WAITING) {
        return start;
    } else {
        UNLOCK(&shm->tree[start].mutex);
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            if (shm->tree[start].child_jobs[i] != -1) {
                int leaf_pos = getLeaf(shm->tree[start].child_jobs[i]);
                if (leaf_pos != -1) {
                    // UNLOCK(&shm->tree[start].mutex);
                    return leaf_pos;
                }
            }
        }
        // UNLOCK(&shm->tree[start].mutex);
        return -1;
    }
}

void *consumer(void *arg) {
    int ind = *(int *)arg;
    while (1) {
        LOCK(&shm->tree[shm->root].mutex);
        if (shm->tree[shm->root].status == DONE) {
            UNLOCK(&shm->tree[shm->root].mutex);
            DEBUG("Consumer %d finished", ind);
            pthread_exit(NULL);
        }
        UNLOCK(&shm->tree[shm->root].mutex);

        int leaf_pos = getLeaf(shm->root);
        if (leaf_pos == -1) {
            continue;
        }

        DEBUG("Consumer %d found leaf %d", ind, leaf_pos);

        shm->tree[leaf_pos].status = ONGOING;
        UNLOCK(&shm->tree[leaf_pos].mutex);
        DEBUG("Consumer %d is working on leaf %d", ind, leaf_pos);
        usleep(shm->tree[leaf_pos].comp_time * 1000);

        LOCK(&shm->mutex);
        shm->tree[leaf_pos].status = DONE;
        shm->node_count--;
        int par_pos = shm->tree[leaf_pos].parent;
        DEBUG("Consumer %d finished leaf %d", ind, leaf_pos);
        UNLOCK(&shm->mutex);

        if (leaf_pos == shm->root) {
            DEBUG("Root finished");
            continue;
        }

        LOCK(&shm->tree[par_pos].mutex);
        int status = shm->tree[par_pos].deleteChild(leaf_pos);
        if (status == -1) {
            // cout << "Error: Failed to delete child from parent" << endl;
            ERROR("Consumer %d failed to delete child %d from parent %d", ind, leaf_pos, par_pos);
        } else {
            DEBUG("Consumer %d deleted child %d from parent %d", ind, leaf_pos, par_pos);
        }
        UNLOCK(&shm->tree[par_pos].mutex);
    }
}

int main() {
    signal(SIGINT, sigintHandler);
    signal(SIGSEGV, sigintHandler);

    srand(time(NULL));
    int np, nc;
    cout << "Enter no. of producer threads: ";
    cin >> np;
    cout << "Enter no. of consumer threads: ";
    cin >> nc;

    shmid = shmget(IPC_PRIVATE, sizeof(SharedMem), IPC_CREAT | 0666);
    shm = (SharedMem *)shmat(shmid, NULL, 0);
    shm->init();

    genInitialTree();

    for (int i = 0; i < shm->node_count; i++) {
        cout << "Node " << i;
        cout << " -> ";
        for (int j = 0; j < MAX_CHILD_JOBS; j++) {
            if (shm->tree[i].child_jobs[j] != -1) {
                cout << shm->tree[i].child_jobs[j] << ", ";
            }
        }
        cout << endl;
    }

    pthread_t prods[np];
    for (int i = 0; i < np; i++) {
        int *ind = new int(i);
        pthread_create(&prods[i], NULL, producer, ind);
    }

    pid_t b_pid = fork();
    if (b_pid == 0) {
        pthread_t cons[nc];
        for (int i = 0; i < nc; i++) {
            int *ind = new int(i);
            pthread_create(&cons[i], NULL, consumer, ind);
        }
        for (int i = 0; i < nc; i++) {
            pthread_join(cons[i], NULL);
        }
        exit(0);
    } else if (b_pid < 0) {
        perror("fork");
        exit(1);
    }

    for (int i = 0; i < np; i++) {
        pthread_join(prods[i], NULL);
    }

    waitpid(b_pid, NULL, 0);

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
}