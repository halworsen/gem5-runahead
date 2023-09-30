#include <iostream>
#include <cstdlib>
#include <string>
#include <cassert>
#include <vector>
#include <random>
#include <algorithm>

// Huge matrix multiplication just to essentially guarantee cache misses
struct Matrix {
private:
    size_t rows;
    size_t columns;
    bool dataAllocated = false;
    long long int *data;
public:
    Matrix(size_t rows, size_t cols) : rows(rows), columns(cols) {
        data = (long long int*) calloc(rows * cols, sizeof(long long int));
        dataAllocated = true;
    }

    ~Matrix() {
        if (dataAllocated)
            free(data);
    }

    size_t getRows() { return rows; }
    size_t getCols() { return columns; }
    long long int get(size_t x, size_t y) { return data[rows * x + y]; }
    void set(size_t x, size_t y, long long int element) { data[rows * x + y] = element; }

    void print() {
        for (int r = 0; r < rows; r++) {
            printf("[ ");
            for (int c = 0; c < columns; c++) {
                printf("%li ", get(r, c));
            }
            printf("]\n");
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

void multiplyMatrices(Matrix *a, Matrix *b, Matrix *out) {
    assert(a->getCols() == b->getRows());

    for (int rA = 0; rA < a->getRows(); rA++) {
        float progress = ((float)rA / (float)a->getRows()) * 100.0f;
        printf("Progress: %f%\n", progress);

        for (int cB = 0; cB < b->getCols(); cB++) {
            for (int cA = 0; cA < a->getCols(); cA++) {
                long long int cell = out->get(rA, cB);
                cell += a->get(rA, cA) * b->get(cA, cB);
                out->set(rA, cB, cell);
            }
        }
    }
}

void multiplyMatricesRandom(Matrix *a, Matrix *b, Matrix *out, unsigned int seed) {
    assert(a->getCols() == b->getRows());

    // make a vector of indices for matrix A's rows/columns
    std::vector<int> aRowIdxs;
    std::vector<int> aColIdxs;
    std::vector<int> bColIdxs;

    for (int rA = 0; rA < a->getRows(); rA++)
        aRowIdxs.push_back(rA);
    for (int cA = 0; cA < b->getCols(); cA++)
        aColIdxs.push_back(cA);
    for (int cB = 0; cB < b->getCols(); cB++)
        bColIdxs.push_back(cB);

    // shuffle
    auto rng = std::default_random_engine(seed);
    std::shuffle(aRowIdxs.begin(), aRowIdxs.end(), rng);
    std::shuffle(aColIdxs.begin(), aColIdxs.end(), rng);
    std::shuffle(bColIdxs.begin(), bColIdxs.end(), rng);

    int prog = 0;
    for (auto i = aRowIdxs.begin(); i != aRowIdxs.end(); i++) {
        int rA = *i;

        float progress = ((float)(prog++) / (float)a->getRows()) * 100.0f;
        printf("Progress: %f%\n", progress);

        for (auto j = bColIdxs.begin(); j != bColIdxs.end(); j++) {
            int cB = *j;
            for (auto k = aColIdxs.begin(); k != aColIdxs.end(); k++) {
                int cA = *k;
                long long int cell = out->get(rA, cB);
                cell += a->get(rA, cA) * b->get(cA, cB);
                out->set(rA, cB, cell);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3 ) {
        printf("Usage: matmul MATRIX_SIZE RANDOM\n");
        return 1;
    }

    int matrixSize = std::stoi(std::string(argv[1]));
    printf("Matrix size: %ix%i\n", matrixSize, matrixSize);

    bool random = (bool) std::stoi(std::string(argv[2]));
    printf("Random: %s\n", random ? "yes" : "no");

    unsigned int seed = 85354712;
    std::srand(seed);

    Matrix matrixA = Matrix(matrixSize, matrixSize);
    Matrix matrixB = Matrix(matrixSize, matrixSize);
    populateMatrix(&matrixA);
    populateMatrix(&matrixB);

    printf("Matrix A:\n");
    matrixA.print();
    printf("Matrix B:\n");
    matrixB.print();

    Matrix matrixC = Matrix(matrixA.getRows(), matrixB.getCols());
    if (random)
        multiplyMatricesRandom(&matrixA, &matrixB, &matrixC, seed);
    else
        multiplyMatrices(&matrixA, &matrixB, &matrixC);

    printf("Result:\n");
    matrixC.print();

    return 0;
}
