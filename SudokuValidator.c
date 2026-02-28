#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <omp.h>

int grid[9][9];

int cols_valid  = 1;
int rows_valid  = 1;
int boxes_valid = 1;

pid_t parent_pid;

int checkColumn(int col) {
    int i, val;
    int seen[10] = {0};
    for (i = 0; i < 9; i++) {
        val = grid[i][col];
        if (val < 1 || val > 9 || seen[val]) return 0;
        seen[val] = 1;
    }
    return 1;
}

int checkRow(int row) {
    int j, val;
    int seen[10] = {0};
    for (j = 0; j < 9; j++) {
        val = grid[row][j];
        if (val < 1 || val > 9 || seen[val]) return 0;
        seen[val] = 1;
    }
    return 1;
}

int checkBox(int startRow, int startCol) {
    int i, j, val;
    int seen[10] = {0};
    for (i = startRow; i < startRow + 3; i++) {
        for (j = startCol; j < startCol + 3; j++) {
            val = grid[i][j];
            if (val < 1 || val > 9 || seen[val]) return 0;
            seen[val] = 1;
        }
    }
    return 1;
}

void run_ps_no_wait(pid_t pid) {
    pid_t child = fork();
    if (child == 0) {
        char pid_str[20];
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        execlp("ps", "ps", "-p", pid_str, "-lLf", NULL);
        perror("execlp");
        exit(1);
    }
}

void run_ps(pid_t pid) {
    pid_t child = fork();
    if (child == 0) {
        char pid_str[20];
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        execlp("ps", "ps", "-p", pid_str, "-lLf", NULL);
        perror("execlp");
        exit(1);
    } else {
        waitpid(child, NULL, 0);
    }
}

void* columnChecker(void* arg) {
    int col;
    pid_t tid = syscall(SYS_gettid);
    printf("El thread que ejecuta el metodo de revision de columnas es: %d\n", tid);

    sleep(1);

    omp_set_num_threads(9);

    #pragma omp parallel for schedule(dynamic) private(tid)
    for (col = 0; col < 9; col++) {
        tid = syscall(SYS_gettid);
        printf("En la revision de columnas el siguiente es un thread en ejecucion: %d\n", tid);
        if (!checkColumn(col)) {
            cols_valid = 0;
        }
    }

    pthread_exit(0);
}

void reviewRows() {
    int row;
    omp_set_num_threads(9);

    #pragma omp parallel for schedule(dynamic)
    for (row = 0; row < 9; row++) {
        if (!checkRow(row)) {
            rows_valid = 0;
        }
    }
}

int main(int argc, char* argv[]) {

    int i, j, k;
    struct stat sb;
    char* mapped;
    int fd;
    pthread_t col_thread;
    pid_t main_tid;
    int starts[3] = {0, 3, 6};

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (fstat(fd, &sb) < 0) {
        perror("fstat");
        close(fd);
        return 1;
    }

    mapped = (char*) mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);

    for (i = 0; i < 9; i++) {
        for (j = 0; j < 9; j++) {
            grid[i][j] = mapped[i * 9 + j] - '0';
        }
    }

    munmap(mapped, sb.st_size);

    for (k = 0; k < 3; k++) {
        if (!checkBox(starts[k], starts[k])) {
            boxes_valid = 0;
        }
    }

    parent_pid = getpid();

    pthread_create(&col_thread, NULL, columnChecker, NULL);

    run_ps_no_wait(parent_pid);

    wait(NULL);
    pthread_join(col_thread, NULL);

    main_tid = syscall(SYS_gettid);
    printf("El thread en el que se ejecuta main es: %d\n", main_tid);

    reviewRows();

    if (cols_valid && rows_valid && boxes_valid) {
        printf("Sudoku resuelto!\n");
    } else {
        printf("Sudoku NO es valido.\n");
    }

    printf("Antes de terminar el estado de este proceso y sus threads es:\n");
    run_ps(parent_pid);

    return 0;
}
