#include <pthread.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include <bitset>
#include <chrono>
#include <iostream>
#include <random>
using namespace std;
using namespace std::chrono;

#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_RESET "\033[0m"

const int QUEUE_SIZE = 3;
const int MAX_JOBS = 2;
const int N = 4;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution<int> dis(1, 100000);

struct Job {
    int prod_num;
    int status;
    int mat[N][N];
    int mat_id;

    Job(int num) {
        // srand(time(NULL));
        prod_num = num;
        status = 0;
        mat_id = rand() % 100000 + 1;
        // mat_id = dis(gen);
    }

    pair<int, int> get_blocks() {
        for (int i = 0; i < 8; i++) {
            if (!(status & (1 << i))) {
                status |= (1 << i);
                return (make_pair(i / 2, i % 4));
            }
        }
        return (make_pair(-1, -1));
    }
};

struct SharedMem {
    Job job_queue[QUEUE_SIZE];
    int in;
    int out;
    int count;
    int job_created;
    int write_ind;
    pthread_mutex_t mutex;

    void init() {
        in = 0;
        out = 0;
        count = 0;
        job_created = 0;
        write_ind = 0;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
    }
};

SharedMem *SHM;

void producer(int num) {
    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        if (SHM->job_created == MAX_JOBS) {
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);

        Job job(num);
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                // job.mat[i][j] = rand() % 19 - 9;
                job.mat[i][j] = i + j;
            }
        }
        // for (int i = 0; i < N; i++) {
        //     for (int j = 0; j < N; j++) {
        //         // job.mat[i][j] = rand() % 19 - 9;
        //         cout << job.mat[i][j] << " ";
        //     }
        //     cout << endl;
        // }
        int ms = rand() % 3001;
        usleep(ms);

        pthread_mutex_lock(&SHM->mutex);
        while (SHM->count == QUEUE_SIZE - 1) {
            pthread_mutex_unlock(&SHM->mutex);
            usleep(1);
            pthread_mutex_lock(&SHM->mutex);
        }
        if (SHM->job_created < MAX_JOBS) {
            SHM->job_queue[SHM->in] = job;
            SHM->in = (SHM->in + 1) % QUEUE_SIZE;
            SHM->count++;
            SHM->job_created++;
            cout << "Prod num: " << job.prod_num << endl;
            cout << "pid: " << getpid() << endl;
            cout << "job_created: " << SHM->job_created << endl;
            cout << "matrix id: " << job.mat_id << endl;
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    cout << job.mat[i][j] << " ";
                }
                cout << endl;
            }
            cout << endl;
        }
        pthread_mutex_unlock(&SHM->mutex);
    }
}

void worker(int num) {
    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        // cout << "****** " << num << " " << SHM->count << " ******" << endl;
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);

        int ms = rand() % 3001;
        usleep(ms);

        pthread_mutex_lock(&SHM->mutex);
        while (SHM->count < 2) {
            pthread_mutex_unlock(&SHM->mutex);
            usleep(1);
            pthread_mutex_lock(&SHM->mutex);
        }

        pair<int, int> blocks = SHM->job_queue[SHM->out].get_blocks();
        if (blocks != make_pair(-1, -1)) {
            if (blocks == make_pair(0, 0)) {
                cout << COLOR_RED << "aa gaya " << SHM->in << COLOR_RESET << endl;
                SHM->write_ind = SHM->in;
                SHM->in = (SHM->in + 1) % QUEUE_SIZE;
                SHM->count++;
                SHM->job_queue[SHM->write_ind].status = 0;
            }
            int aind = SHM->out;
            int bind = (aind + 1) % QUEUE_SIZE;
            pthread_mutex_unlock(&SHM->mutex);

            pthread_mutex_lock(&SHM->mutex);
            int p = 2 * ((blocks.first & 2) + (blocks.second & 1));

            cout << "Worker num: " << num << endl;
            cout << "Producers: " << SHM->job_queue[aind].prod_num << " " << SHM->job_queue[bind].prod_num << endl;
            cout << "Indices: " << aind << " " << bind << endl;
            cout << "Matrix ids: " << SHM->job_queue[aind].mat_id << " " << SHM->job_queue[bind].mat_id << endl;
            cout << "Blocks: " << blocks.first << " " << blocks.second << endl;
            cout << "p: " << p << " " << p + 1 << endl;
            cout << "status: " << bitset<8>(SHM->job_queue[SHM->out].status) << endl;
            cout << "Reading" << endl;
            cout << endl;
            pthread_mutex_unlock(&SHM->mutex);

            int pmat[N / 2][N / 2];
            for (int i = 0; i < N / 2; i++) {
                for (int j = 0; j < N / 2; j++) {
                    pmat[i][j] = 0;
                    for (int k = 0; k < N / 2; k++) {
                        int acol = k + N / 2 * (blocks.first & 1);
                        int arow = i + N / 2 * (blocks.first / 2);
                        int bcol = j + N / 2 * (blocks.second & 1);
                        int brow = k + N / 2 * (blocks.second / 2);
                        pmat[i][j] += SHM->job_queue[aind].mat[arow][acol] * SHM->job_queue[bind].mat[brow][bcol];
                    }
                }
            }

            pthread_mutex_lock(&SHM->mutex);

            cout << "Worker num: " << num << endl;
            cout << "Producers: " << SHM->job_queue[aind].prod_num << " " << SHM->job_queue[bind].prod_num << endl;
            cout << "Indices: " << aind << " " << bind << endl;
            cout << "Matrix ids: " << SHM->job_queue[aind].mat_id << " " << SHM->job_queue[bind].mat_id << endl;
            cout << "Blocks: " << blocks.first << " " << blocks.second << endl;
            cout << "p: " << p << " " << p + 1 << endl;
            cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << endl;
            cout << "Write ind status: " << bitset<8>(SHM->job_queue[SHM->write_ind].status) << endl;
            if (!(SHM->job_queue[SHM->write_ind].status & (1 << p)) && !(SHM->job_queue[SHM->write_ind].status & (1 << (p + 1)))) {
                // copy
                cout << "Copying" << endl;
                cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << endl;
                cout << endl;
                for (int i = 0; i < N / 2; i++) {
                    for (int j = 0; j < N / 2; j++) {
                        int crow = i + (N / 2) * (blocks.first / 2);
                        int ccol = j + (N / 2) * (blocks.second & 1);
                        if (SHM->write_ind == 5) {
                            cout << COLOR_RED << i << " " << j << COLOR_RESET << endl;
                        }
                        cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << endl;
                        int x = SHM->write_ind;
                        cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << " " << x << endl;
                        cout << crow << " " << ccol << " " << pmat[i][j] << endl;
                        SHM->job_queue[x].mat[crow][ccol] = pmat[i][j];
                        cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << " " << x << endl;
                    }
                }
                cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << endl;
                SHM->job_queue[SHM->write_ind].status |= (1 << p);
                cout << COLOR_GREEN << __LINE__ << " " << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << " write index: " << SHM->write_ind << endl;

            } else if (!(SHM->job_queue[SHM->write_ind].status & (1 << (p + 1)))) {
                // add
                cout << "Adding" << endl;
                cout << endl;
                for (int i = 0; i < N / 2; i++) {
                    for (int j = 0; j < N / 2; j++) {
                        int crow = i + N / 2 * (blocks.first / 2);
                        int ccol = j + N / 2 * (blocks.second & 1);
                        SHM->job_queue[SHM->write_ind].mat[crow][ccol] += pmat[i][j];
                    }
                }
                SHM->job_queue[SHM->write_ind].status |= (1 << (p + 1));
            }

            cout << COLOR_GREEN << getpid() << " " << &(SHM->write_ind) << COLOR_RESET << "write index: " << SHM->write_ind << endl;
            cout << "hello " << bitset<8>(SHM->job_queue[SHM->write_ind].status) << endl;
            if (SHM->job_queue[SHM->write_ind].status == (1 << 8) - 1) {
                cout << "*** haan aayaaaaaaaa ***" << endl;
                SHM->job_queue[SHM->write_ind].status = 0;
                SHM->out = (SHM->out + 2) % QUEUE_SIZE;
                SHM->count -= 2;
            }
            pthread_mutex_unlock(&SHM->mutex);
        }
        pthread_mutex_unlock(&SHM->mutex);
    }
}

int main() {
    srand(time(NULL));

    int NP, NW;
    cout << "Enter number of producers: ";
    cin >> NP;
    cout << "Enter number of workers: ";
    cin >> NW;

    auto start = high_resolution_clock::now();

    int shmid = shmget(IPC_PRIVATE, sizeof(SharedMem), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    SHM = (SharedMem *)shmat(shmid, NULL, 0);
    if (SHM == (void *)-1) {
        perror("shmat");
        exit(1);
    }
    SHM->init();

    for (int i = 1; i <= NP; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            srand(time(NULL) * getpid());
            producer(i);
            cout << "****** prod " << i << " gaya ******" << endl;
            exit(0);
        }
    }

    for (int i = 1; i <= NW; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            srand(time(NULL) * getpid());
            worker(-i);
            cout << "****** worker " << i << " gaya ******" << endl;
            exit(0);
        }
    }

    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        // cout << "**************** " << SHM->count << " ****************" << endl;
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            cout << endl;
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    cout << SHM->job_queue[SHM->out].mat[i][j] << " ";
                }
                cout << endl;
            }
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(end - start);
            cout << "Time taken " << duration.count() << " milliseconds" << endl;
            pthread_mutex_unlock(&SHM->mutex);
            pthread_mutex_destroy(&SHM->mutex);
            shmdt(SHM);
            shmctl(shmid, IPC_RMID, NULL);
            kill(0, SIGKILL);
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);
    }

    // destroy shm
    pthread_mutex_destroy(&SHM->mutex);
    shmdt(SHM);
    shmctl(shmid, IPC_RMID, NULL);
}

// compute

// while(!all flag set) {
//     unlock;
//     lock;
// }
// write at ind

// check for flag -> no lock
// if(!all flag set) { do work }

/*
D000 = A00 * B00
D001 = A01 * B10
D010 = A00 * B01
D011 = A01 * B11
D100 = A10 * B00
D101 = A11 * B10
D110 = A10 * B01
D111 = A11 * B11

A00 (0) - B00 (0), B01 (1)
A01 (1) - B10 (2), B11 (3)
A10 (2) - B00 (0), B01 (1)
A11 (3) - B10 (2), B11 (3)

{i/2, i%4}

00 00 00 00

0 0 - 0
0 1 - 1
1 2 - 0
1 3 - 1

2 0 - 2
2 1 - 3
3 2 - 2
3 3 - 3

first & 2  + second & 1
*/