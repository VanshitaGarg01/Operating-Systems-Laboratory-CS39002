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
    int prod_num;                   // Producer number that created this job
    int status;                     // stores the status of matrix multiplication block-wise
    long long mat[N][N];            // Matrix
    int mat_id;                     // Matrix ID, is a random integer

    Job(int num) {
        prod_num = num;
        status = 0;
        mat_id = rand() % 100000 + 1;
    }

    // returns a pair of integers, first is a block from the first matrix, 
    // second is a block from the second matrix that is to be multiplied
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
    Job job_queue[QUEUE_SIZE];      // Job queue 
    int in;                         // Index of the next job to be added
    int out;                        // Index of the next job to be removed
    int count;                      // Number of jobs in the queue
    int job_created;                // Number of jobs created by the producers
    int write_ind;                  // Index of the next job to be written to the shared memory
    pthread_mutex_t mutex;          // Mutex to protect the shared memory

    // initilize the job queue
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

// Shared memory
SharedMem *SHM;

// prints the newly created job details to the console
void producer_out(Job &job) {
    cout << endl;
    cout << "Producer number: " << job.prod_num << endl;
    cout << "pid: " << getpid() << endl;
    cout << "job_created: " << SHM->job_created << endl;
    cout << "Matrix ID: " << job.mat_id << endl;
}

// prints the worker process details to the console
void consumer_out(int num, int a_ind, int b_ind, pair<int, int> blocks) {
    cout << endl;
    cout << "Worker number: " << num << endl;
    cout << "Producer numbers: " << SHM->job_queue[a_ind].prod_num << ", " << SHM->job_queue[b_ind].prod_num << endl;
    cout << "Matrix IDs: " << SHM->job_queue[a_ind].mat_id << " " << SHM->job_queue[b_ind].mat_id << endl;
    cout << "Blocks: " << bitset<2>(blocks.first) << " " << bitset<2>(blocks.second) << endl;
}

// this function is called by the producer to create a new job,
// wait for a random interval of time between 0 and 3 seconds,
// and add it to the job queue
void producer(int num) {
    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        if (SHM->job_created == MAX_JOBS) {
            // if the maximum number of jobs has been created, then exit
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);

        // create a new job to be added to the job queue
        Job job(num);
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                // job.mat[i][j] = (i == j ? 2 : 0);
                // job.mat[i][j] = i + j;
                job.mat[i][j] = rand() % 19 - 9;
            }
        }

        // sleep for a random interval of time between 0 and 3 seconds
        int ms = (rand() % MAX_SLEEP) * 1000;
        usleep(ms);

        pthread_mutex_lock(&SHM->mutex);
        // spin until the job queue is full
        while (SHM->count >= QUEUE_SIZE - 1) {
            pthread_mutex_unlock(&SHM->mutex);
            usleep(1);
            pthread_mutex_lock(&SHM->mutex);
        }
        // add the new job to the job queue
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

// this function is called by the workers to process a job,
// wait for a random interval of time between 0 and 3 seconds,
// retrieves two blocks of the first two matrices in the queue, and
// updates the status of the job
void worker(int num) {
    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        // if maximum number of jobs has been created by producers and
        // count of jobs in the SHM queue is 1, then exit
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }
        pthread_mutex_unlock(&SHM->mutex);

        // sleep for a random interval of time between 0 and 3 seconds
        int ms = (rand() % MAX_SLEEP) * 1000;
        usleep(ms);

        pthread_mutex_lock(&SHM->mutex);
        // spin until the size of job queue < 2
        while (SHM->count < 2 && !(SHM->job_created == MAX_JOBS)) {
            pthread_mutex_unlock(&SHM->mutex);
            usleep(1);
            pthread_mutex_lock(&SHM->mutex);
        }
        // if maximum number of jobs has been created by producers and
        // count of jobs in the SHM queue is 1, then exit
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            pthread_mutex_unlock(&SHM->mutex);
            break;
        }

        // get the two unprocessed blocks of the first two matrices in the queue
        pair<int, int> blocks = SHM->job_queue[SHM->out].get_blocks();

        // if there are blocks to be processed, then process them
        if (blocks != make_pair(-1, -1)) {
            // if matrices A and B are accessed for the first time, then
            // create space for the product matrix in the queue and increment in
            if (blocks == make_pair(0, 0)) {
                SHM->write_ind = SHM->in;
                SHM->in = (SHM->in + 1) % QUEUE_SIZE;
                SHM->count++;
                SHM->job_queue[SHM->write_ind].prod_num = num;
                SHM->job_queue[SHM->write_ind].mat_id = rand() % 100000 + 1;
                SHM->job_queue[SHM->write_ind].status = 0;
            }
            
            int a_ind = SHM->out;                   // index of matrix A
            int b_ind = (a_ind + 1) % QUEUE_SIZE;   // index of matrix B
            pthread_mutex_unlock(&SHM->mutex);

            pthread_mutex_lock(&SHM->mutex);
            // p is the block of matrix C which is to be updated
            int p = 2 * ((blocks.first & 2) + (blocks.second & 1));

            consumer_out(num, a_ind, b_ind, blocks);
            cout << "Reading" << endl;
            pthread_mutex_unlock(&SHM->mutex);

            // compute the product of the two blocks and store it in pmat
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
            
            //  copy the contents of pmat into block p of the product matrix in the queue
            //  if block p is being accessed for the first time
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

            } 
            //  add the contents of pmat to block p of the product matrix in the queue
            //  if block p had already being accessed before
            else if (!(SHM->job_queue[SHM->write_ind].status & (1 << (p + 1)))) {
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

            // if all blocks of the product matrix in the queue are processed, then
            // decrement the count of jobs in the SHM queue and increment out
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

    // initilize shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedMem), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    // attach shared memory
    SHM = (SharedMem *)shmat(shmid, NULL, 0);
    if (SHM == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    // initialize the shared memory queue
    SHM->init();

    vector<pid_t> producers, workers;

    // create producers
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

    // create workers
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
    // wait for all producers and workers to finish
    while (1) {
        pthread_mutex_lock(&SHM->mutex);
        if (SHM->count == 1 && SHM->job_created == MAX_JOBS) {
            // calculate the trace of the product matrix finally remaining in the queue
            long long trace = 0;
            for (int i = 0; i < N; i++) {
                trace += SHM->job_queue[SHM->out].mat[i][i];
            }
            cout << endl
                 << "Trace: " << trace << endl;
            auto end = high_resolution_clock::now();
            // calculate the time taken by the program
            auto duration = duration_cast<milliseconds>(end - start);
            // print the time taken by the program
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

    // detach shared memory
    shmdt(SHM);

    // remove shared memory
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}