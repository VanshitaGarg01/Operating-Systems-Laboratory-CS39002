#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>

using namespace std;

typedef struct _process_data {
    double** A;
    double** B;
    double** C;
    int veclen, i, j;
} ProcessData;

void printMatrix (double** mat, int r, int c) {
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            cout << mat[i][j] << " ";
        }
        cout << endl;
    }
    cout << endl;
    return;
}

double** get_matrix (int r, int c, int& shmid, int shmids[]) {
    shmid = shmget(IPC_PRIVATE, r * sizeof(double*), IPC_CREAT | 0666);
    double** mat = (double**) shmat(shmid, NULL, 0);
    for (int i = 0; i < r; i++) {
        shmids[i] = shmget(IPC_PRIVATE, c * sizeof (double), IPC_CREAT | 0666);
        mat[i] = (double *) shmat(shmids[i], NULL, 0);
        for (int j = 0; j < c; j++) {
            mat[i][j] = ((double) rand() / RAND_MAX) * (double)10.0;
            // mat[i][j] = i+j;
        }
    }
    cout << endl;
    return mat;
}

void* mult (void* arg) {
    ProcessData* data = (ProcessData*) arg;
    data->C[data->i][data->j] = 0;
    for (int i = 0; i < data->veclen; i++) {
        data->C[data->i][data->j] += data->A[data->i][i] * data->B[i][data->j];
    }
    return NULL;
}

void destroy (int shmid, int shmids[], int n, double** &mat) {
    for (int i = 0; i < n; i++) {
        shmdt(mat[i]);
        shmctl(shmids[i], IPC_RMID, NULL);
    }
    shmdt(mat);
    shmctl(shmid, IPC_RMID, NULL);
}

int main() {
    int r1, r2, c1, c2;
    cout << "Enter the number of rows of matrix A: ";
    cin >> r1;
    cout << "Enter the number of columns of matrix A: ";
    cin >> c1;
    cout << "Enter the number of rows of matrix B: ";
    cin >> r2;
    cout << "Enter the number of columns of matrix B: ";
    cin >> c2;

    // check c1 == r2??

    int shmid1, shmid2, shmid3;
    int shmids_A[r1], shmids_B[r2], shmids_C[r1];
    double** A = get_matrix(r1, c1, shmid1, shmids_A);
    double** B = get_matrix(r2, c2, shmid2, shmids_B);
    double** C = get_matrix(r1, c2, shmid3, shmids_C);
    
    printMatrix(A, r1, c1);
    printMatrix(B, r2, c2);

    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c2; j++) {
            pid_t pid;
            pid = fork();
            if (pid) {
                // parent
                continue;
            } else {
                // child
                ProcessData* data = new ProcessData;
                data->A = A;
                data->B = B;
                data->C = C;
                data->veclen = c1;
                data->i = i;
                data->j = j;
                mult(data);
                delete(data);
                exit(0);
            }
        }
    }

    while (wait(NULL) > 0);

    printMatrix(C, r1, c2);
    
    destroy(shmid1, shmids_A, r1, A);
    destroy(shmid2, shmids_B, r2, B);
    destroy(shmid3, shmids_C, r1, C);

    return 0;
}