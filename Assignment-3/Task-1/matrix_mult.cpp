#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>

using namespace std;

typedef struct _process_data {
    double** A;
    double** B;
    double** C;
    int veclen, i, j;

    _process_data(double** a, double** b, double** c, int v, int i, int j) {
        A = a;
        B = b;
        C = c;
        veclen = v;
        this->i = i;
        this->j = j;
    }
} ProcessData;

void print_matrix(double** mat, int r, int c) {
    cout << fixed << setprecision(4);
    cout << "[";
    for (int i = 0; i < r; i++) {
        cout << "[";
        for (int j = 0; j < c; j++) {
            cout << mat[i][j];
            if (j != c - 1) {
                cout << ", ";
            }
        }
        cout << (i != r - 1 ? "],\n" : "]");
    }
    cout << "]" << endl;
    return;
}

double** get_matrix(int r, int c, int& shmid, int shmids[]) {
    shmid = shmget(IPC_PRIVATE, r * sizeof(double*), IPC_CREAT | 0666);
    double** mat = (double**)shmat(shmid, NULL, 0);
    for (int i = 0; i < r; i++) {
        shmids[i] = shmget(IPC_PRIVATE, c * sizeof(double), IPC_CREAT | 0666);
        mat[i] = (double*)shmat(shmids[i], NULL, 0);
        for (int j = 0; j < c; j++) {
            mat[i][j] = ((double)rand() / RAND_MAX) * (double)10.0;
        }
    }
    return mat;
}

void* mult(void* arg) {
    ProcessData* data = (ProcessData*)arg;
    data->C[data->i][data->j] = 0;
    for (int i = 0; i < data->veclen; i++) {
        data->C[data->i][data->j] += data->A[data->i][i] * data->B[i][data->j];
    }
    return NULL;
}

void destroy(int shmid, int shmids[], int n, double**& mat) {
    for (int i = 0; i < n; i++) {
        shmdt(mat[i]);
        shmctl(shmids[i], IPC_RMID, NULL);
    }
    shmdt(mat);
    shmctl(shmid, IPC_RMID, NULL);
}

int main() {
    int r1, r2, c1, c2;
    cout << "Enter the number of rows of matrix A (r1): ";
    cin >> r1;
    cout << "Enter the number of columns of matrix A (c1): ";
    cin >> c1;
    cout << "Enter the number of rows of matrix B (r2): ";
    cin >> r2;
    cout << "Enter the number of columns of matrix B (c2): ";
    cin >> c2;

    if (c1 != r2) {
        cout << "Cannot multiply matrices, c1 should be equal to r2" << endl;
        exit(1);
    }

    int shmid1, shmid2, shmid3;
    int shmids_A[r1], shmids_B[r2], shmids_C[r1];
    double** A = get_matrix(r1, c1, shmid1, shmids_A);
    double** B = get_matrix(r2, c2, shmid2, shmids_B);
    double** C = get_matrix(r1, c2, shmid3, shmids_C);

    cout << endl
         << "Matrix A:" << endl;
    print_matrix(A, r1, c1);
    cout << endl
         << "Matrix B:" << endl;
    print_matrix(B, r2, c2);

    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c2; j++) {
            pid_t pid = fork();
            if (pid == 0) {
                ProcessData* data = new ProcessData(A, B, C, c1, i, j);
                mult(data);
                delete data;
                exit(0);
            }
        }
    }

    while (wait(NULL) > 0)
        ;

    cout << endl
         << "Matrix C:" << endl;
    print_matrix(C, r1, c2);
    cout << endl;

    // double mat[r1][c2];
    // for (int i = 0; i < r1; i++) {
    //     for (int j = 0; j < c2; j++) {
    //         mat[i][j] = 0;
    //         for (int k = 0; k < r2; k++) {
    //             mat[i][j] += A[i][k] * B[k][j];
    //         }
    //     }
    // }

    // for (int i = 0; i < r1; i++) {
    //     for (int j = 0; j < c2; j++) {
    //         if (abs(mat[i][j] - C[i][j]) > 1e-5) {
    //             cout << "Error: " << mat[i][j] << " != " << C[i][j] << endl;
    //         }
    //     }
    // }

    destroy(shmid1, shmids_A, r1, A);
    destroy(shmid2, shmids_B, r2, B);
    destroy(shmid3, shmids_C, r1, C);

    return 0;
}