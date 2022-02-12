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

// const int MAX_SLEEP = 3000001;
const int MAX_SLEEP = 3;
const int QUEUE_SIZE = 8;
const int MAX_JOBS = 10;
const int N = 10;

#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN "\033[1;36m"
#define COLOR_RESET "\033[0m"

struct Job {
    int prod_num;
    int status;
    long long mat[N][N];
    int mat_id;

    Job(int num) {
        prod_num = num;
        status = 0;
        mat_id = rand() % 100000 + 1;
    }

    pair<int, int> get_blocks() {
        for (int i = 0; i < 8; i++) {
            if (!(status & (1 << i))) {
                status |= (1 << i);
                return {i / 2, i % 4};
            }
        }
        return {-1, -1};
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
                job.mat[i][j] = rand() % 19 - 9;
                // job.mat[i][j] = i + j;
            }
        }
        int ms = rand() % MAX_SLEEP;
        usleep(ms);

        pthread_mutex_lock(&SHM->mutex);
        while (SHM->count >= QUEUE_SIZE - 1) {
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
            // for (int i = 0; i < N; i++) {
            //     for (int j = 0; j < N; j++) {
            //         cout << job.mat[i][j] << " ";
            //     }
            //     cout << endl;
            // }
            cout << endl;
        }
        pthread_mutex_unlock(&SHM->mutex);
    }
}

void worker(int num) {
    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);

        int ms = rand() % MAX_SLEEP;
        usleep(ms);

        pthread_mutex_lock(&SHM->mutex);
        while (SHM->count < 2 && !(SHM->job_created == MAX_JOBS)) {
            pthread_mutex_unlock(&SHM->mutex);
            usleep(1);
            pthread_mutex_lock(&SHM->mutex);
        }
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }

        pair<int, int> blocks = SHM->job_queue[SHM->out].get_blocks();
        if (blocks != make_pair(-1, -1)) {
            if (blocks == make_pair(0, 0)) {
                SHM->write_ind = SHM->in;
                SHM->in = (SHM->in + 1) % QUEUE_SIZE;
                SHM->count++;
                // Job job(num);
                // SHM->job_queue[SHM->write_ind] = job;
                SHM->job_queue[SHM->write_ind].prod_num = num;
                SHM->job_queue[SHM->write_ind].mat_id = rand() % 100000 + 1;
                SHM->job_queue[SHM->write_ind].status = 0;
            }
            int aind = SHM->out;
            int bind = (aind + 1) % QUEUE_SIZE;
            pthread_mutex_unlock(&SHM->mutex);

            pthread_mutex_lock(&SHM->mutex);
            int p = 2 * ((blocks.first & 2) + (blocks.second & 1));

            cout << __LINE__ << " " << getpid() << " " << "Worker num: " << num << endl;
            cout << __LINE__ << " " << getpid() << " " << "Producers: " << SHM->job_queue[aind].prod_num << " " << SHM->job_queue[bind].prod_num << endl;
            cout << __LINE__ << " " << getpid() << " " << "Indices: " << aind << " " << bind << endl;
            cout << __LINE__ << " " << getpid() << " " << "Matrix ids: " << SHM->job_queue[aind].mat_id << " " << SHM->job_queue[bind].mat_id << endl;
            cout << __LINE__ << " " << getpid() << " " << "Blocks: " << blocks.first << " " << blocks.second << endl;
            cout << __LINE__ << " " << getpid() << " " << "p: " << p << " " << p + 1 << endl;
            cout << __LINE__ << " " << getpid() << " " << "status: " << bitset<8>(SHM->job_queue[SHM->out].status) << endl;
            cout << __LINE__ << " " << getpid() << " " << "count: " << SHM->count << endl;
            cout << __LINE__ << " " << getpid() << " " << "Reading" << endl;
            cout << endl;
            pthread_mutex_unlock(&SHM->mutex);

            long long pmat[N / 2][N / 2];
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

            cout << __LINE__ << " " << getpid() << " " << "Worker num: " << num << endl;
            cout << __LINE__ << " " << getpid() << " " << "Producers: " << SHM->job_queue[aind].prod_num << " " << SHM->job_queue[bind].prod_num << endl;
            cout << __LINE__ << " " << getpid() << " " << "Indices: " << aind << " " << bind << endl;
            cout << __LINE__ << " " << getpid() << " " << "Matrix ids: " << SHM->job_queue[aind].mat_id << " " << SHM->job_queue[bind].mat_id << endl;
            cout << __LINE__ << " " << getpid() << " " << "Blocks: " << blocks.first << " " << blocks.second << endl;
            cout << __LINE__ << " " << getpid() << " " << "p: " << p << " " << p + 1 << endl;
            cout << __LINE__ << " " << getpid() << " " << "count: " << SHM->count << endl;
            cout << __LINE__ << " " << getpid() << " " << "Write ind: " << SHM->write_ind << endl;
            cout << __LINE__ << " " << getpid() << " " << "Write ind status: " << bitset<8>(SHM->job_queue[SHM->write_ind].status) << endl;
            
            if (!(SHM->job_queue[SHM->write_ind].status & (1 << p)) && !(SHM->job_queue[SHM->write_ind].status & (1 << (p + 1)))) {
                // copy
                cout << "Copying" << endl;
                cout << endl;
                for (int i = 0; i < N / 2; i++) {
                    for (int j = 0; j < N / 2; j++) {
                        int crow = i + (N / 2) * (blocks.first / 2);
                        int ccol = j + (N / 2) * (blocks.second & 1);
                        SHM->job_queue[SHM->write_ind].mat[crow][ccol] = pmat[i][j];
                    }
                }
                SHM->job_queue[SHM->write_ind].status |= (1 << p);

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

            if (SHM->job_queue[SHM->write_ind].status == (1 << 8) - 1) {
                SHM->job_queue[SHM->write_ind].status = 0;
                SHM->out = (SHM->out + 2) % QUEUE_SIZE;
                SHM->count -= 2;
                cout << COLOR_RED << "*** khatam ***" << COLOR_RESET << endl;
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
            exit(0);
        }
    }

    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            // cout << endl;
            // for (int i = 0; i < N; i++) {
            //     for (int j = 0; j < N; j++) {
            //         cout << SHM->job_queue[SHM->out].mat[i][j] << " ";
            //     }
            //     cout << endl;
            // }
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