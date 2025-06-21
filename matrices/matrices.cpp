#include "matrices.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <stdexcept>

void Matrix::allocate(size_t r, size_t c)
{
    rows = r;
    cols = c;
    data = new (std::nothrow) double[rows * cols]();
    if (!data)
        error = MATRIX_ERR_ALLOC;
}

void Matrix::deallocate()
{
    delete[] data;
    data = nullptr;
    rows = cols = 0;
}

void Matrix::copyFrom(const Matrix &other)
{
    allocate(other.rows, other.cols);
    if (data)
    {
        std::memcpy(data, other.data, sizeof(double) * rows * cols);
        error = other.error;
    }
}

Matrix::Matrix(size_t r, size_t c) : rows(r), cols(c), error(MATRIX_SUCCESS)
{
    data = new double[rows * cols]();
}

Matrix::~Matrix()
{
    delete[] data;
}

Matrix::Matrix(const Matrix &other) : rows(other.rows), cols(other.cols), error(other.error)
{
    data = new double[rows * cols];
    std::memcpy(data, other.data, sizeof(double) * rows * cols);
}

Matrix::Matrix() : rows(0), cols(0), data(nullptr), error(MATRIX_SUCCESS) {}

Matrix::Matrix(Matrix &&other) noexcept : rows(0), cols(0), data(nullptr), error(MATRIX_SUCCESS)
{
    moveFrom(other);
}

Matrix &Matrix::operator=(const Matrix &other)
{
    if (this != &other)
    {
        delete[] data;
        rows = other.rows;
        cols = other.cols;
        error = other.error;
        data = new double[rows * cols];
        std::memcpy(data, other.data, sizeof(double) * rows * cols);
    }
    return *this;
}

Matrix &Matrix::operator=(Matrix &&other) noexcept
{
    if (this != &other)
    {
        delete[] data;
        moveFrom(other);
    }
    return *this;
}

void Matrix::moveFrom(Matrix &other)
{
    rows = other.rows;
    cols = other.cols;
    data = other.data;
    error = other.error;
    other.data = nullptr;
    other.rows = other.cols = 0;
}

double &Matrix::at(size_t r, size_t c)
{
    return data[r * cols + c];
}

const double &Matrix::at(size_t r, size_t c) const
{
    return data[r * cols + c];
}

const char *Matrix::getErrorMessage() const
{
    switch (error)
    {
    case MATRIX_SUCCESS:
        return "Success";
    case MATRIX_ERR_ALLOC:
        return "Allocation failed";
    case MATRIX_ERR_DIM_MISMATCH:
        return "Dimension mismatch";
    case MATRIX_ERR_IO:
        return "Input/Output error";
    case MATRIX_ERR_INVALID:
        return "Invalid operation";
    default:
        return "Unknown error";
    }
}

bool Matrix::operator==(const Matrix &other) const
{
    if (rows != other.rows || cols != other.cols)
        return false;
    for (size_t i = 0; i < rows * cols; ++i)
    {
        if (data[i] != other.data[i])
            return false;
    }
    return true;
}

Matrix Matrix::operator+(const Matrix &other) const
{
    return add(other);
}

Matrix Matrix::operator-(const Matrix &other) const
{
    return subtract(other);
}

Matrix Matrix::operator*(const Matrix &other) const
{
    return multiply(other);
}

Matrix Matrix::add(const Matrix &other) const
{
    if (rows != other.rows || cols != other.cols)
    {
        Matrix result;
        result.error = MATRIX_ERR_DIM_MISMATCH;
        return result;
    }
    Matrix result(rows, cols);
    for (size_t i = 0; i < rows * cols; ++i)
        result.data[i] = data[i] + other.data[i];
    return result;
}

Matrix Matrix::subtract(const Matrix &other) const
{
    if (rows != other.rows || cols != other.cols)
    {
        Matrix result;
        result.error = MATRIX_ERR_DIM_MISMATCH;
        return result;
    }
    Matrix result(rows, cols);
    for (size_t i = 0; i < rows * cols; ++i)
        result.data[i] = data[i] - other.data[i];
    return result;
}

Matrix Matrix::multiply(const Matrix &other) const
{
    if (cols != other.rows)
    {
        Matrix result;
        result.error = MATRIX_ERR_DIM_MISMATCH;
        return result;
    }
    Matrix result(rows, other.cols);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < other.cols; ++j)
            for (size_t k = 0; k < cols; ++k)
                result.at(i, j) += at(i, k) * other.at(k, j);
    return result;
}

Matrix Matrix::transpose() const
{
    Matrix result(cols, rows);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            result.at(j, i) = at(i, j);
    return result;
}

bool Matrix::saveToText(const char *filename) const
{
    FILE *f = fopen(filename, "w");
    if (!f)
        return false;
    fprintf(f, "%zu %zu\n", rows, cols);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            fprintf(f, "%.10lf ", at(i, j));
    fclose(f);
    return true;
}

bool Matrix::saveToBinary(const char *filename) const
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        return false;
    fwrite(&rows, sizeof(size_t), 1, f);
    fwrite(&cols, sizeof(size_t), 1, f);
    fwrite(data, sizeof(double), rows * cols, f);
    fclose(f);
    return true;
}

Matrix Matrix::loadFromText(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
        return Matrix();
    size_t r, c;
    fscanf(f, "%zu %zu", &r, &c);
    Matrix result(r, c);
    for (size_t i = 0; i < r; ++i)
        for (size_t j = 0; j < c; ++j)
            fscanf(f, "%lf", &result.at(i, j));
    fclose(f);
    return result;
}

Matrix Matrix::loadFromBinary(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    if (!f)
        return Matrix();
    size_t r, c;
    fread(&r, sizeof(size_t), 1, f);
    fread(&c, sizeof(size_t), 1, f);
    Matrix result(r, c);
    fread(result.data, sizeof(double), r * c, f);
    fclose(f);
    return result;
}

void Matrix::print(FILE *stream) const
{
    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t j = 0; j < cols; ++j)
            fprintf(stream, "%.2lf ", at(i, j));
        fprintf(stream, "\n");
    }
}

Matrix Matrix::read(FILE *stream)
{
    size_t r, c;
    fscanf(stream, "%zu %zu", &r, &c);
    Matrix result(r, c);
    for (size_t i = 0; i < r; ++i)
        for (size_t j = 0; j < c; ++j)
            fscanf(stream, "%lf", &result.at(i, j));
    return result;
}
size_t Matrix::numRows() const
{
    return rows;
}

size_t Matrix::numCols() const
{
    return cols;
}

MatrixError Matrix::getError() const
{
    return error;
}

void Matrix::clearError()
{
    error = MATRIX_SUCCESS;
}

Matrix Matrix::scalarMultiply(double scalar) const
{
    Matrix result(rows, cols);
    for (size_t i = 0; i < rows * cols; ++i)
        result.data[i] = data[i] * scalar;
    return result;
}

Matrix Matrix::operator*(double scalar) const
{
    return scalarMultiply(scalar);
}

Matrix operator*(double scalar, const Matrix &mat)
{
    return mat.scalarMultiply(scalar);
}

Matrix Matrix::resize(size_t newRows, size_t newCols) const
{
    Matrix result(newRows, newCols);
    for (size_t i = 0; i < std::min(rows, newRows); ++i)
        for (size_t j = 0; j < std::min(cols, newCols); ++j)
            result.at(i, j) = at(i, j);
    return result;
}

void Matrix::swapRows(size_t row1, size_t row2)
{
    if (row1 >= rows || row2 >= rows)
        return;
    for (size_t j = 0; j < cols; ++j)
        std::swap(at(row1, j), at(row2, j));
}

void Matrix::scaleRow(size_t row, double scalar)
{
    if (row >= rows)
        return;
    for (size_t j = 0; j < cols; ++j)
        at(row, j) *= scalar;
}

void Matrix::addRows(size_t srcRow, size_t destRow, double scalar)
{
    if (srcRow >= rows || destRow >= rows)
        return;
    for (size_t j = 0; j < cols; ++j)
        at(destRow, j) += at(srcRow, j) * scalar;
}

void Matrix::swapCols(size_t col1, size_t col2)
{
    if (col1 >= cols || col2 >= cols)
        return;
    for (size_t i = 0; i < rows; ++i)
        std::swap(at(i, col1), at(i, col2));
}

void Matrix::scaleCol(size_t col, double scalar)
{
    if (col >= cols)
        return;
    for (size_t i = 0; i < rows; ++i)
        at(i, col) *= scalar;
}

void Matrix::addCols(size_t srcCol, size_t destCol, double scalar)
{
    if (srcCol >= cols || destCol >= cols)
        return;
    for (size_t i = 0; i < rows; ++i)
        at(i, destCol) += at(i, srcCol) * scalar;
}

Matrix Matrix::cofactor() const
{
    if (rows != cols)
    {
        Matrix result;
        result.error = MATRIX_ERR_INVALID;
        return result;
    }
    Matrix result(rows, cols);
    for (size_t i = 0; i < rows; ++i)
    {
        for (size_t j = 0; j < cols; ++j)
        {
            Matrix sub(rows - 1, cols - 1);
            for (size_t r = 0, sr = 0; r < rows; ++r)
            {
                if (r == i)
                    continue;
                for (size_t c = 0, sc = 0; c < cols; ++c)
                {
                    if (c == j)
                        continue;
                    sub.at(sr, sc++) = at(r, c);
                }
                sr++;
            }
            result.at(i, j) = std::pow(-1, i + j) * sub.determinant();
        }
    }
    return result;
}

double Matrix::determinant() const
{
    if (rows != cols)
        return 0.0;
    if (rows == 1)
        return at(0, 0);
    if (rows == 2)
        return at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0);

    double det = 0.0;
    for (size_t j = 0; j < cols; ++j)
    {
        Matrix sub(rows - 1, cols - 1);
        for (size_t r = 1; r < rows; ++r)
            for (size_t c = 0, sc = 0; c < cols; ++c)
                if (c != j)
                    sub.at(r - 1, sc++) = at(r, c);
        det += std::pow(-1, j) * at(0, j) * sub.determinant();
    }
    return det;
}

Matrix Matrix::adjoint() const
{
    return cofactor().transpose();
}

Matrix Matrix::inverse() const
{
    double det = determinant();
    if (det == 0.0)
    {
        Matrix result;
        result.error = MATRIX_ERR_INVALID;
        return result;
    }
    return adjoint().scalarMultiply(1.0 / det);
}
