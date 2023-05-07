#include <iostream>
#include <cstdlib>
#include <string>
#include <cassert>

// Huge matrix multiplication just to essentially guarantee cache misses
struct Matrix {
private:
    size_t rows;
    size_t columns;
    long long int *data;
public:
    Matrix(size_t rows, size_t cols) : rows(rows), columns(cols) {
        data = (long long int*) calloc(rows * cols, sizeof(long long int));
    }
    ~Matrix() { free(data); }

    size_t getRows() { return rows; }
    size_t getCols() { return columns; }
    long long int get(size_t x, size_t y) { return data[rows * x + y]; }
    void set(size_t x, size_t y, long long int element) { data[rows * x + y] = element; }

    void print() {
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < columns; c++) {
                printf("%li ", get(r, c));
            }
            printf("\n");
        }
    }
};

// Populate a matrix with random values
void populateMatrix(Matrix *matrix) {
    for (int r = 0; r < matrix->getRows(); r++) {
        for (int c = 0; c < matrix->getCols(); c++) {
            matrix->set(r, c, (std::rand() % 1000000) - 500000);
        }
    }
}

Matrix multiplyMatrices(Matrix *a, Matrix *b) {
    assert(a->getCols() == b->getRows());

    Matrix out = Matrix(a->getRows(), b->getCols());
    for (int rA = 0; rA < a->getRows(); rA++) {
        float progress = ((float)rA / (float)a->getRows()) * 100.0f;
        printf("Progress: %f%\n", progress);

        for (int cB = 0; cB < b->getCols(); cB++) {
            for (int cA = 0; cA < a->getCols(); cA++) {
                long long int cell = out.get(rA, cB);
                cell += a->get(rA, cA) * b->get(cA, cB);
                out.set(rA, cB, cell);
            }
        }
    }

    return out;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: matmul MATRIX_SIZE\n");
        return 1;
    }

    int matrixSize = std::stoi(std::string(argv[1]));
    printf("Matrix size: %ix%i\n", matrixSize, matrixSize);

    unsigned int seed = 85354712; // time(NULL)
    std::srand(seed);

    Matrix matrixA = Matrix(matrixSize, matrixSize);
    Matrix matrixB = Matrix(matrixSize, matrixSize);
    populateMatrix(&matrixA);
    populateMatrix(&matrixB);

    //printf("Matrix A:\n");
    //matrixA.print();
    //printf("Matrix B:\n");
    //matrixB.print();

    Matrix matrixC = multiplyMatrices(&matrixA, &matrixB);

    //printf("Result:\n");
    //matrixC.print();

    printf("C[0][0] = %li\n", matrixC.get(0, 0));

    return 0;
}
