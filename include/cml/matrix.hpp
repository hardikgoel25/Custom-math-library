// cml/matrix.hpp — dense row-major matrices with bounds-checked access and
// numerically stable factorisations.
//
// Every operation that can fail throws (std::invalid_argument for shape
// errors, std::out_of_range for bad indices, std::length_error for sizes
// above the configured cap, std::runtime_error for singular systems), so
// callers can never silently read garbage.
#ifndef CML_MATRIX_HPP
#define CML_MATRIX_HPP

#include <cstddef>
#include <initializer_list>
#include <iosfwd>
#include <string>
#include <vector>

namespace cml {

class Matrix {
public:
    // Size cap guards against allocation bombs from untrusted input.
    static std::size_t max_elements();
    static void set_max_elements(std::size_t n);

    Matrix();
    Matrix(std::size_t rows, std::size_t cols, double fill = 0.0);
    Matrix(std::initializer_list<std::initializer_list<double>> rows);
    explicit Matrix(const std::vector<std::vector<double>>& rows);
    static Matrix identity(std::size_t n);
    static Matrix zeros(std::size_t rows, std::size_t cols);
    static Matrix ones(std::size_t rows, std::size_t cols);
    static Matrix diagonal(const std::vector<double>& d);
    static Matrix column(const std::vector<double>& v);
    static Matrix row_vector(const std::vector<double>& v);
    static Matrix parse(const std::string& text); // "1 2; 3 4" or "[[1,2],[3,4]]"

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    bool is_square() const { return rows_ == cols_; }
    bool is_symmetric(double tol = 1e-12) const;

    double& operator()(std::size_t r, std::size_t c);
    double  operator()(std::size_t r, std::size_t c) const;
    double& at(std::size_t r, std::size_t c) { return (*this)(r, c); }
    double  at(std::size_t r, std::size_t c) const { return (*this)(r, c); }
    const std::vector<double>& data() const { return data_; }
    std::vector<double> row(std::size_t r) const;
    std::vector<double> col(std::size_t c) const;
    std::vector<std::vector<double>> to_rows() const;

    // Arithmetic -----------------------------------------------------------
    Matrix operator-() const;
    Matrix& operator+=(const Matrix& o);
    Matrix& operator-=(const Matrix& o);
    Matrix& operator*=(double s);
    Matrix& operator/=(double s);
    Matrix hadamard(const Matrix& o) const;      // element-wise product
    Matrix transpose() const;
    Matrix multiply(const Matrix& o) const;      // naive O(n^3)
    Matrix multiply_strassen(const Matrix& o, std::size_t leaf = 64) const;
    Matrix pow(unsigned n) const;                // square matrices
    double trace() const;
    double norm_frobenius() const;
    double norm_1() const;
    double norm_inf() const;
    double sum() const;
    double min() const;
    double max() const;

    // Row / column operations -------------------------------------------
    void swap_rows(std::size_t a, std::size_t b);
    void scale_row(std::size_t r, double s);
    void add_row_multiple(std::size_t src, std::size_t dst, double s); // dst += s*src
    void swap_cols(std::size_t a, std::size_t b);
    void scale_col(std::size_t c, double s);
    void add_col_multiple(std::size_t src, std::size_t dst, double s);
    Matrix submatrix(std::size_t r0, std::size_t c0, std::size_t nrows, std::size_t ncols) const;
    Matrix minor_matrix(std::size_t r, std::size_t c) const; // drop row r and column c
    Matrix resized(std::size_t rows, std::size_t cols) const; // crop/zero-pad
    Matrix hstack(const Matrix& o) const;
    Matrix vstack(const Matrix& o) const;

    // Linear algebra -------------------------------------------------------
    struct LU;      // PA = LU with partial pivoting
    struct QR;      // A = QR (Householder)
    struct Eigen;   // eigenvalues / eigenvectors
    struct RREF;    // reduced row echelon form

    LU lu() const;
    QR qr() const;                       // Householder
    Matrix cholesky() const;             // lower L with A = L L^T; throws if not SPD
    RREF rref(double tol = 1e-12) const;
    std::size_t rank(double tol = 1e-12) const;
    double determinant() const;
    Matrix inverse() const;              // throws std::runtime_error if singular
    Matrix cofactor_matrix() const;
    Matrix adjugate() const;
    std::vector<double> solve(const std::vector<double>& b) const;      // A x = b via LU
    Matrix solve(const Matrix& B) const;                                // multiple right-hand sides
    std::vector<double> solve_least_squares(const std::vector<double>& b) const; // via QR
    double condition_number() const;     // 1-norm based, inf if singular
    Eigen eigen_symmetric(double tol = 1e-12, int max_sweeps = 100) const;  // Jacobi
    Eigen eigenvalues(double tol = 1e-12, int max_iter = 1000) const;       // Hessenberg + shifted QR
    std::pair<double, std::vector<double>> power_iteration(int max_iter = 1000, double tol = 1e-12) const;

    // Formatting -----------------------------------------------------------
    std::string to_string(int precision = 6) const;
    bool approx_equal(const Matrix& o, double tol = 1e-9) const;

private:
    std::size_t rows_ = 0, cols_ = 0;
    std::vector<double> data_;
    void check_same_shape(const Matrix& o, const char* op) const;
    void check_square(const char* op) const;
};

struct Matrix::LU {
    Matrix L, U;                   // unit lower, upper
    std::vector<std::size_t> perm; // PA = LU: perm[i] is the original row at position i
    int sign = 1;                  // permutation parity (+1 / -1)
    bool singular = false;
};
struct Matrix::QR {
    Matrix Q, R;
};
struct Matrix::Eigen {
    std::vector<double> values;    // real eigenvalues (symmetric) or real parts
    std::vector<double> imag;      // imaginary parts (general case; zeros if symmetric)
    Matrix vectors;                // columns are eigenvectors (symmetric case only)
    int iterations = 0;
    bool converged = false;
};
struct Matrix::RREF {
    Matrix matrix;
    std::vector<std::size_t> pivots; // pivot column per pivot row
    std::size_t rank = 0;
};

Matrix operator+(const Matrix& a, const Matrix& b);
Matrix operator-(const Matrix& a, const Matrix& b);
Matrix operator*(const Matrix& a, const Matrix& b);
Matrix operator*(const Matrix& a, double s);
Matrix operator*(double s, const Matrix& a);
Matrix operator/(const Matrix& a, double s);
std::vector<double> operator*(const Matrix& a, const std::vector<double>& v);
bool operator==(const Matrix& a, const Matrix& b);
bool operator!=(const Matrix& a, const Matrix& b);
std::ostream& operator<<(std::ostream& os, const Matrix& m);

} // namespace cml

#endif // CML_MATRIX_HPP
