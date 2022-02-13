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
const int MAX_SLEEP = 30;
const int QUEUE_SIZE = 9;
const int MAX_JOBS = 10;
const int N = 1000;

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

void producer_out(Job &job) {
    cout << endl;
    cout << "Producer number: " << job.prod_num << endl;
    cout << "pid: " << getpid() << endl;
    cout << "job_created: " << SHM->job_created << endl;
    cout << "Matrix ID: " << job.mat_id << endl;
}

void consumer_out(int num, int a_ind, int b_ind, pair<int, int> blocks) {
    cout << endl;
    cout << "Worker number: " << num << endl;
    cout << "Producer numbers: " << SHM->job_queue[a_ind].prod_num << ", " << SHM->job_queue[b_ind].prod_num << endl;
    cout << "Matrix IDs: " << SHM->job_queue[a_ind].mat_id << " " << SHM->job_queue[b_ind].mat_id << endl;
    cout << "Blocks: " << bitset<2>(blocks.first) << " " << bitset<2>(blocks.second) << endl;
}

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
                job.mat[i][j] = (i == j ? 2 : 0);
                // job.mat[i][j] = i + j;
            }
        }
        int ms = (rand() % MAX_SLEEP) * 1000;
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
            producer_out(job);
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

        int ms = (rand() % MAX_SLEEP) * 1000;
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
                SHM->job_queue[SHM->write_ind].prod_num = num;
                SHM->job_queue[SHM->write_ind].mat_id = rand() % 100000 + 1;
                SHM->job_queue[SHM->write_ind].status = 0;
            }
            int a_ind = SHM->out;
            int b_ind = (a_ind + 1) % QUEUE_SIZE;
            pthread_mutex_unlock(&SHM->mutex);

            pthread_mutex_lock(&SHM->mutex);
            int p = 2 * ((blocks.first & 2) + (blocks.second & 1));

            consumer_out(num, a_ind, b_ind, blocks);
            cout << "Reading" << endl;
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
                        pmat[i][j] += SHM->job_queue[a_ind].mat[arow][acol] * SHM->job_queue[b_ind].mat[brow][bcol];
                    }
                }
            }

            pthread_mutex_lock(&SHM->mutex);

            consumer_out(num, a_ind, b_ind, blocks);
            if (!(SHM->job_queue[SHM->write_ind].status & (1 << p)) && !(SHM->job_queue[SHM->write_ind].status & (1 << (p + 1)))) {
                // copy
                cout << "Copying" << endl;
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
                cout << endl << "One step done" << endl;
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

    vector<pid_t> producers, workers;

    for (int i = 1; i <= NP; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            srand(time(NULL) * getpid());
            producer(i);
            exit(0);
        } else {
            producers.push_back(pid);
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
        } else {
            workers.push_back(pid);
        }
    }

    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            long long trace = 0;
            for (int i = 0; i < N; i++) {
                trace += SHM->job_queue[SHM->out].mat[i][i];
            }
            cout << endl
                 << "Trace: " << trace << endl;
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(end - start);
            cout << "Time taken " << duration.count() << " ms" << endl;
            pthread_mutex_unlock(&SHM->mutex);
            pthread_mutex_destroy(&SHM->mutex);
            for (pid_t pid : producers) {
                kill(pid, SIGKILL);
            }
            for (pid_t pid : workers) {
                kill(pid, SIGKILL);
            }
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);
    }

    shmdt(SHM);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}