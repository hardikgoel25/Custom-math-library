#ifndef MATRIX_H_INCLUDED
#define MATRIX_H_INCLUDED

#include <iostream>
#include <cstdio>
#include <cstring>

enum MatrixError
{
    MATRIX_SUCCESS = 0,
    MATRIX_ERR_ALLOC,
    MATRIX_ERR_DIM_MISMATCH,
    MATRIX_ERR_IO,
    MATRIX_ERR_INVALID
};

class Matrix
{
private:
    size_t rows;
    size_t cols;
    double *data;
    MatrixError error;

    void allocate(size_t r, size_t c);
    void deallocate();
    void copyFrom(const Matrix &other);
    void moveFrom(Matrix &other);

public:
    // Constructors and Destructor
    Matrix();
    Matrix(size_t rows, size_t cols);
    Matrix(const Matrix &other);
    Matrix(Matrix &&other) noexcept;
    ~Matrix();

    // Assignment Operators
    Matrix &operator=(const Matrix &other);
    Matrix &operator=(Matrix &&other) noexcept;

    // Element Access
    double &at(size_t row, size_t col);
    const double &at(size_t row, size_t col) const;

    // Accessors
    size_t numRows() const;
    size_t numCols() const;
    MatrixError getError() const;
    const char *getErrorMessage() const;
    void clearError();

    // Comparison
    bool operator==(const Matrix &other) const;

    // Arithmetic Operators
    Matrix operator+(const Matrix &other) const;
    Matrix operator-(const Matrix &other) const;
    Matrix operator*(const Matrix &other) const;
    Matrix operator*(double scalar) const;

    // Friend Operator
    friend Matrix operator*(double scalar, const Matrix &mat);
    friend std::ostream &operator<<(std::ostream &out, const Matrix &mat);

    // Functional Alternatives
    Matrix add(const Matrix &other) const;
    Matrix subtract(const Matrix &other) const;
    Matrix multiply(const Matrix &other) const;
    Matrix scalarMultiply(double scalar) const;

    // Matrix Manipulation
    Matrix resize(size_t newRows, size_t newCols) const;
    Matrix transpose() const;
    Matrix cofactor() const;
    Matrix adjoint() const;
    Matrix inverse() const;
    double determinant() const;

    // Row and Column Operations
    void swapRows(size_t row1, size_t row2);
    void scaleRow(size_t row, double scalar);
    void addRows(size_t srcRow, size_t destRow, double scalar);

    void swapCols(size_t col1, size_t col2);
    void scaleCol(size_t col, double scalar);
    void addCols(size_t srcCol, size_t destCol, double scalar);

    // Strassen multiplication interfaces
    Matrix multiplyStrassen(const Matrix &other) const;
    Matrix multiplyStrassenRecursive(const Matrix &other) const;
    Matrix multiplyStrassenPowerOf2(const Matrix &other) const;

    // File I/O
    bool saveToText(const char *filename) const;
    bool saveToBinary(const char *filename) const;
    static Matrix loadFromText(const char *filename);
    static Matrix loadFromBinary(const char *filename);

    // Stream I/O
    void print(FILE *stream = stdout) const;
    static Matrix read(FILE *stream);
};

#endif // MATRIX_H_INCLUDED
