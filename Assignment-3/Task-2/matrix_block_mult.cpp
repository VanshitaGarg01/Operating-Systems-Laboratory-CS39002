#include <pthread.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
using namespace std;

const int QUEUE_SIZE = 8;
const int MAX_JOBS = 10;
const int N = 1000;

struct Job {
    int prod_num;
    int status;
    int matrix[N][N];
    int matrix_id;
};

struct SharedMem {
    Job job_buffer[4 * QUEUE_SIZE];
    int in;
    int out;
    int count;
    int job_created;
    pthread_mutex_t mutex;
    int ind;

    void init() {
        in = 0;
        out = 0;
        count = 0;
        job_created = 0;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
    }
};

void producer(int num) {
}

void worker() {
    // compute

    // while(!all flag set) {
    //     unlock;
    //     lock;
    // }
    // write at ind

    // check for flag -> no lock
    // if(!all flag set) { do work }

}

int main() {
    int NP, NW;
    cout << "Enter number of producers: ";
    cin >> NP;
    cout << "Enter number of workers: ";
    cin >> NW;

    int shmid = shmget(IPC_PRIVATE, sizeof(SharedMem), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    SharedMem *SHM = (SharedMem *)shmat(shmid, NULL, 0);
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
            // producer(i);
        }
    }

    for (int i = 1; i <= NW; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // worker();
        }
    }

    // destroy shm
    pthread_mutex_destroy(&SHM->mutex);
    shmdt(SHM);
    shmctl(shmid, IPC_RMID, NULL);
}