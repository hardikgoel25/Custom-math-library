// cml/matrix.cpp — dense matrices and numerically stable linear algebra.
#include "cml/matrix.hpp"
#include "cml/core.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace cml {

namespace {
std::size_t g_max_elements = 4000000; // 2000 x 2000 doubles = 32 MB

std::size_t checked_size(std::size_t rows, std::size_t cols) {
    if (rows == 0 || cols == 0) return 0;
    if (rows > g_max_elements / cols) throw std::length_error("matrix size exceeds the configured limit");
    const std::size_t n = rows * cols;
    if (n > g_max_elements) throw std::length_error("matrix size exceeds the configured limit");
    return n;
}
} // namespace

std::size_t Matrix::max_elements() { return g_max_elements; }
void Matrix::set_max_elements(std::size_t n) { g_max_elements = n; }

Matrix::Matrix() {}
Matrix::Matrix(std::size_t rows, std::size_t cols, double fill) : rows_(rows), cols_(cols) {
    const std::size_t n = checked_size(rows, cols);
    if (n == 0) { rows_ = cols_ = 0; return; }
    data_.assign(n, fill);
}
Matrix::Matrix(std::initializer_list<std::initializer_list<double>> rows) {
    rows_ = rows.size();
    cols_ = rows_ ? rows.begin()->size() : 0;
    checked_size(rows_, cols_);
    data_.reserve(rows_ * cols_);
    for (const auto& r : rows) {
        if (r.size() != cols_) throw std::invalid_argument("ragged matrix initializer");
        data_.insert(data_.end(), r.begin(), r.end());
    }
    if (cols_ == 0) rows_ = 0;
}
Matrix::Matrix(const std::vector<std::vector<double>>& rows) {
    rows_ = rows.size();
    cols_ = rows_ ? rows[0].size() : 0;
    checked_size(rows_, cols_);
    data_.reserve(rows_ * cols_);
    for (const auto& r : rows) {
        if (r.size() != cols_) throw std::invalid_argument("ragged matrix rows");
        data_.insert(data_.end(), r.begin(), r.end());
    }
    if (cols_ == 0) rows_ = 0;
}
Matrix Matrix::identity(std::size_t n) {
    Matrix m(n, n, 0.0);
    for (std::size_t i = 0; i < n; ++i) m(i, i) = 1.0;
    return m;
}
Matrix Matrix::zeros(std::size_t r, std::size_t c) { return Matrix(r, c, 0.0); }
Matrix Matrix::ones(std::size_t r, std::size_t c) { return Matrix(r, c, 1.0); }
Matrix Matrix::diagonal(const std::vector<double>& d) {
    Matrix m(d.size(), d.size(), 0.0);
    for (std::size_t i = 0; i < d.size(); ++i) m(i, i) = d[i];
    return m;
}
Matrix Matrix::column(const std::vector<double>& v) {
    Matrix m(v.size(), 1, 0.0);
    for (std::size_t i = 0; i < v.size(); ++i) m(i, 0) = v[i];
    return m;
}
Matrix Matrix::row_vector(const std::vector<double>& v) {
    Matrix m(1, v.size(), 0.0);
    for (std::size_t i = 0; i < v.size(); ++i) m(0, i) = v[i];
    return m;
}

Matrix Matrix::parse(const std::string& text) {
    if (text.size() > 2000000) throw std::invalid_argument("matrix text too long");
    std::vector<std::vector<double>> rows;
    std::vector<double> current;
    std::string token;
    auto flush_token = [&]() {
        if (token.empty()) return;
        char* end = nullptr;
        const double v = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0') throw std::invalid_argument("invalid number '" + token + "' in matrix");
        current.push_back(v);
        token.clear();
    };
    auto flush_row = [&]() {
        flush_token();
        if (!current.empty()) { rows.push_back(current); current.clear(); }
    };
    for (char c : text) {
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E') { token.push_back(c); continue; }
        if (c == ',' || c == ' ' || c == '\t') { flush_token(); continue; }
        if (c == ';' || c == '\n' || c == ']' || c == '\r') { flush_row(); continue; }
        if (c == '[') continue;
        throw std::invalid_argument(std::string("unexpected character '") + c + "' in matrix");
    }
    flush_row();
    if (rows.empty()) throw std::invalid_argument("empty matrix");
    return Matrix(rows);
}

bool Matrix::is_symmetric(double tol) const {
    if (!is_square()) return false;
    for (std::size_t i = 0; i < rows_; ++i)
        for (std::size_t j = i + 1; j < cols_; ++j)
            if (abs((*this)(i, j) - (*this)(j, i)) > tol) return false;
    return true;
}

double& Matrix::operator()(std::size_t r, std::size_t c) {
    if (r >= rows_ || c >= cols_) throw std::out_of_range("matrix index out of range");
    return data_[r * cols_ + c];
}
double Matrix::operator()(std::size_t r, std::size_t c) const {
    if (r >= rows_ || c >= cols_) throw std::out_of_range("matrix index out of range");
    return data_[r * cols_ + c];
}
std::vector<double> Matrix::row(std::size_t r) const {
    if (r >= rows_) throw std::out_of_range("row index out of range");
    return std::vector<double>(data_.begin() + static_cast<std::ptrdiff_t>(r * cols_),
                               data_.begin() + static_cast<std::ptrdiff_t>((r + 1) * cols_));
}
std::vector<double> Matrix::col(std::size_t c) const {
    if (c >= cols_) throw std::out_of_range("column index out of range");
    std::vector<double> v(rows_);
    for (std::size_t i = 0; i < rows_; ++i) v[i] = data_[i * cols_ + c];
    return v;
}
std::vector<std::vector<double>> Matrix::to_rows() const {
    std::vector<std::vector<double>> out(rows_);
    for (std::size_t i = 0; i < rows_; ++i) out[i] = row(i);
    return out;
}

void Matrix::check_same_shape(const Matrix& o, const char* op) const {
    if (rows_ != o.rows_ || cols_ != o.cols_)
        throw std::invalid_argument(std::string("dimension mismatch in ") + op + ": " + std::to_string(rows_) + "x" +
                                    std::to_string(cols_) + " vs " + std::to_string(o.rows_) + "x" + std::to_string(o.cols_));
}
void Matrix::check_square(const char* op) const {
    if (!is_square() || empty()) throw std::invalid_argument(std::string(op) + " requires a non-empty square matrix");
}

// Arithmetic ------------------------------------------------------------------
Matrix Matrix::operator-() const {
    Matrix r = *this;
    for (double& v : r.data_) v = -v;
    return r;
}
Matrix& Matrix::operator+=(const Matrix& o) {
    check_same_shape(o, "addition");
    for (std::size_t i = 0; i < data_.size(); ++i) data_[i] += o.data_[i];
    return *this;
}
Matrix& Matrix::operator-=(const Matrix& o) {
    check_same_shape(o, "subtraction");
    for (std::size_t i = 0; i < data_.size(); ++i) data_[i] -= o.data_[i];
    return *this;
}
Matrix& Matrix::operator*=(double s) {
    for (double& v : data_) v *= s;
    return *this;
}
Matrix& Matrix::operator/=(double s) {
    for (double& v : data_) v /= s;
    return *this;
}
Matrix Matrix::hadamard(const Matrix& o) const {
    check_same_shape(o, "hadamard product");
    Matrix r = *this;
    for (std::size_t i = 0; i < data_.size(); ++i) r.data_[i] *= o.data_[i];
    return r;
}
Matrix Matrix::transpose() const {
    Matrix r(cols_, rows_, 0.0);
    for (std::size_t i = 0; i < rows_; ++i)
        for (std::size_t j = 0; j < cols_; ++j) r.data_[j * rows_ + i] = data_[i * cols_ + j];
    return r;
}
Matrix Matrix::multiply(const Matrix& o) const {
    if (cols_ != o.rows_)
        throw std::invalid_argument("dimension mismatch in multiplication: " + std::to_string(rows_) + "x" +
                                    std::to_string(cols_) + " * " + std::to_string(o.rows_) + "x" + std::to_string(o.cols_));
    Matrix r(rows_, o.cols_, 0.0);
    for (std::size_t i = 0; i < rows_; ++i) {
        for (std::size_t k = 0; k < cols_; ++k) {
            const double a = data_[i * cols_ + k];
            if (a == 0.0) continue;
            const double* brow = &o.data_[k * o.cols_];
            double* rrow = &r.data_[i * o.cols_];
            for (std::size_t j = 0; j < o.cols_; ++j) rrow[j] += a * brow[j];
        }
    }
    return r;
}

namespace {
std::size_t next_pow2(std::size_t n) {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}
Matrix strassen_rec(const Matrix& A, const Matrix& B, std::size_t leaf) {
    const std::size_t n = A.rows();
    if (n <= leaf) return A.multiply(B);
    const std::size_t h = n / 2;
    const Matrix A11 = A.submatrix(0, 0, h, h), A12 = A.submatrix(0, h, h, h);
    const Matrix A21 = A.submatrix(h, 0, h, h), A22 = A.submatrix(h, h, h, h);
    const Matrix B11 = B.submatrix(0, 0, h, h), B12 = B.submatrix(0, h, h, h);
    const Matrix B21 = B.submatrix(h, 0, h, h), B22 = B.submatrix(h, h, h, h);
    const Matrix M1 = strassen_rec(A11 + A22, B11 + B22, leaf);
    const Matrix M2 = strassen_rec(A21 + A22, B11, leaf);
    const Matrix M3 = strassen_rec(A11, B12 - B22, leaf);
    const Matrix M4 = strassen_rec(A22, B21 - B11, leaf);
    const Matrix M5 = strassen_rec(A11 + A12, B22, leaf);
    const Matrix M6 = strassen_rec(A21 - A11, B11 + B12, leaf);
    const Matrix M7 = strassen_rec(A12 - A22, B21 + B22, leaf);
    const Matrix C11 = M1 + M4 - M5 + M7;
    const Matrix C12 = M3 + M5;
    const Matrix C21 = M2 + M4;
    const Matrix C22 = M1 - M2 + M3 + M6;
    Matrix C(n, n, 0.0);
    for (std::size_t i = 0; i < h; ++i)
        for (std::size_t j = 0; j < h; ++j) {
            C(i, j) = C11(i, j);
            C(i, j + h) = C12(i, j);
            C(i + h, j) = C21(i, j);
            C(i + h, j + h) = C22(i, j);
        }
    return C;
}
} // namespace

Matrix Matrix::multiply_strassen(const Matrix& o, std::size_t leaf) const {
    if (cols_ != o.rows_) throw std::invalid_argument("dimension mismatch in multiplication");
    if (leaf < 2) leaf = 2;
    const std::size_t n = next_pow2(std::max(rows_, std::max(cols_, o.cols_)));
    if (n <= leaf) return multiply(o);
    const Matrix A = resized(n, n), B = o.resized(n, n);
    return strassen_rec(A, B, leaf).submatrix(0, 0, rows_, o.cols_);
}

Matrix Matrix::pow(unsigned n) const {
    check_square("matrix power");
    Matrix result = identity(rows_);
    Matrix base = *this;
    while (n) {
        if (n & 1) result = result.multiply(base);
        n >>= 1;
        if (n) base = base.multiply(base);
    }
    return result;
}
double Matrix::trace() const {
    check_square("trace");
    double t = 0.0;
    for (std::size_t i = 0; i < rows_; ++i) t += (*this)(i, i);
    return t;
}
double Matrix::norm_frobenius() const {
    double s = 0.0;
    for (double v : data_) s += v * v;
    return sqrt(s);
}
double Matrix::norm_1() const {
    double best = 0.0;
    for (std::size_t j = 0; j < cols_; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < rows_; ++i) s += abs((*this)(i, j));
        best = fmax(best, s);
    }
    return best;
}
double Matrix::norm_inf() const {
    double best = 0.0;
    for (std::size_t i = 0; i < rows_; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < cols_; ++j) s += abs((*this)(i, j));
        best = fmax(best, s);
    }
    return best;
}
double Matrix::sum() const {
    double s = 0.0;
    for (double v : data_) s += v;
    return s;
}
double Matrix::min() const {
    if (empty()) return nan();
    double m = data_[0];
    for (double v : data_) m = fmin(m, v);
    return m;
}
double Matrix::max() const {
    if (empty()) return nan();
    double m = data_[0];
    for (double v : data_) m = fmax(m, v);
    return m;
}

// Row / column operations -----------------------------------------------------
void Matrix::swap_rows(std::size_t a, std::size_t b) {
    if (a >= rows_ || b >= rows_) throw std::out_of_range("row index out of range");
    if (a == b) return;
    for (std::size_t j = 0; j < cols_; ++j) std::swap(data_[a * cols_ + j], data_[b * cols_ + j]);
}
void Matrix::scale_row(std::size_t r, double s) {
    if (r >= rows_) throw std::out_of_range("row index out of range");
    for (std::size_t j = 0; j < cols_; ++j) data_[r * cols_ + j] *= s;
}
void Matrix::add_row_multiple(std::size_t src, std::size_t dst, double s) {
    if (src >= rows_ || dst >= rows_) throw std::out_of_range("row index out of range");
    for (std::size_t j = 0; j < cols_; ++j) data_[dst * cols_ + j] += s * data_[src * cols_ + j];
}
void Matrix::swap_cols(std::size_t a, std::size_t b) {
    if (a >= cols_ || b >= cols_) throw std::out_of_range("column index out of range");
    if (a == b) return;
    for (std::size_t i = 0; i < rows_; ++i) std::swap(data_[i * cols_ + a], data_[i * cols_ + b]);
}
void Matrix::scale_col(std::size_t c, double s) {
    if (c >= cols_) throw std::out_of_range("column index out of range");
    for (std::size_t i = 0; i < rows_; ++i) data_[i * cols_ + c] *= s;
}
void Matrix::add_col_multiple(std::size_t src, std::size_t dst, double s) {
    if (src >= cols_ || dst >= cols_) throw std::out_of_range("column index out of range");
    for (std::size_t i = 0; i < rows_; ++i) data_[i * cols_ + dst] += s * data_[i * cols_ + src];
}
Matrix Matrix::submatrix(std::size_t r0, std::size_t c0, std::size_t nrows, std::size_t ncols) const {
    if (r0 + nrows > rows_ || c0 + ncols > cols_) throw std::out_of_range("submatrix out of range");
    Matrix m(nrows, ncols, 0.0);
    for (std::size_t i = 0; i < nrows; ++i)
        for (std::size_t j = 0; j < ncols; ++j) m.data_[i * ncols + j] = data_[(r0 + i) * cols_ + c0 + j];
    return m;
}
Matrix Matrix::minor_matrix(std::size_t r, std::size_t c) const {
    if (r >= rows_ || c >= cols_) throw std::out_of_range("minor index out of range");
    if (rows_ < 2 || cols_ < 2) return Matrix();
    Matrix m(rows_ - 1, cols_ - 1, 0.0);
    for (std::size_t i = 0, mi = 0; i < rows_; ++i) {
        if (i == r) continue;
        for (std::size_t j = 0, mj = 0; j < cols_; ++j) {
            if (j == c) continue;
            m(mi, mj++) = (*this)(i, j);
        }
        ++mi;
    }
    return m;
}
Matrix Matrix::resized(std::size_t rows, std::size_t cols) const {
    Matrix m(rows, cols, 0.0);
    for (std::size_t i = 0; i < std::min(rows, rows_); ++i)
        for (std::size_t j = 0; j < std::min(cols, cols_); ++j) m(i, j) = (*this)(i, j);
    return m;
}
Matrix Matrix::hstack(const Matrix& o) const {
    if (rows_ != o.rows_) throw std::invalid_argument("hstack requires equal row counts");
    Matrix m(rows_, cols_ + o.cols_, 0.0);
    for (std::size_t i = 0; i < rows_; ++i) {
        for (std::size_t j = 0; j < cols_; ++j) m(i, j) = (*this)(i, j);
        for (std::size_t j = 0; j < o.cols_; ++j) m(i, cols_ + j) = o(i, j);
    }
    return m;
}
Matrix Matrix::vstack(const Matrix& o) const {
    if (cols_ != o.cols_) throw std::invalid_argument("vstack requires equal column counts");
    Matrix m(rows_ + o.rows_, cols_, 0.0);
    std::copy(data_.begin(), data_.end(), m.data_.begin());
    std::copy(o.data_.begin(), o.data_.end(), m.data_.begin() + static_cast<std::ptrdiff_t>(data_.size()));
    return m;
}

// Linear algebra ----------------------------------------------------------------
Matrix::LU Matrix::lu() const {
    check_square("LU decomposition");
    const std::size_t n = rows_;
    LU out;
    out.U = *this;
    out.L = identity(n);
    out.perm.resize(n);
    for (std::size_t i = 0; i < n; ++i) out.perm[i] = i;
    for (std::size_t k = 0; k < n; ++k) {
        // partial pivoting
        std::size_t p = k;
        double best = abs(out.U(k, k));
        for (std::size_t i = k + 1; i < n; ++i) {
            const double v = abs(out.U(i, k));
            if (v > best) { best = v; p = i; }
        }
        if (best == 0.0) { out.singular = true; continue; }
        if (p != k) {
            out.U.swap_rows(p, k);
            std::swap(out.perm[p], out.perm[k]);
            for (std::size_t j = 0; j < k; ++j) std::swap(out.L(p, j), out.L(k, j));
            out.sign = -out.sign;
        }
        for (std::size_t i = k + 1; i < n; ++i) {
            const double f = out.U(i, k) / out.U(k, k);
            out.L(i, k) = f;
            if (f == 0.0) continue;
            for (std::size_t j = k; j < n; ++j) out.U(i, j) -= f * out.U(k, j);
            out.U(i, k) = 0.0;
        }
    }
    return out;
}

Matrix::QR Matrix::qr() const {
    if (empty()) throw std::invalid_argument("QR of empty matrix");
    const std::size_t m = rows_, n = cols_;
    QR out;
    out.R = *this;
    out.Q = identity(m);
    const std::size_t steps = std::min(m - 1, n);
    for (std::size_t k = 0; k < steps; ++k) {
        // Householder vector for column k below the diagonal.
        double norm = 0.0;
        for (std::size_t i = k; i < m; ++i) norm += out.R(i, k) * out.R(i, k);
        norm = sqrt(norm);
        if (norm == 0.0) continue;
        const double alpha = out.R(k, k) > 0 ? -norm : norm;
        std::vector<double> v(m, 0.0);
        for (std::size_t i = k; i < m; ++i) v[i] = out.R(i, k);
        v[k] -= alpha;
        double vnorm = 0.0;
        for (std::size_t i = k; i < m; ++i) vnorm += v[i] * v[i];
        if (vnorm == 0.0) continue;
        // R = (I - 2 v v^T / v^T v) R
        for (std::size_t j = 0; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k; i < m; ++i) dot += v[i] * out.R(i, j);
            const double f = 2.0 * dot / vnorm;
            for (std::size_t i = k; i < m; ++i) out.R(i, j) -= f * v[i];
        }
        // Q = Q (I - 2 v v^T / v^T v)
        for (std::size_t i = 0; i < m; ++i) {
            double dot = 0.0;
            for (std::size_t l = k; l < m; ++l) dot += out.Q(i, l) * v[l];
            const double f = 2.0 * dot / vnorm;
            for (std::size_t l = k; l < m; ++l) out.Q(i, l) -= f * v[l];
        }
        for (std::size_t i = k + 1; i < m; ++i) out.R(i, k) = 0.0;
    }
    return out;
}

Matrix Matrix::cholesky() const {
    check_square("Cholesky decomposition");
    if (!is_symmetric(1e-9)) throw std::runtime_error("Cholesky requires a symmetric matrix");
    const std::size_t n = rows_;
    Matrix L(n, n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
        double s = (*this)(j, j);
        for (std::size_t k = 0; k < j; ++k) s -= L(j, k) * L(j, k);
        if (s <= 0.0) throw std::runtime_error("matrix is not positive definite");
        L(j, j) = sqrt(s);
        for (std::size_t i = j + 1; i < n; ++i) {
            double t = (*this)(i, j);
            for (std::size_t k = 0; k < j; ++k) t -= L(i, k) * L(j, k);
            L(i, j) = t / L(j, j);
        }
    }
    return L;
}

Matrix::RREF Matrix::rref(double tol) const {
    RREF out;
    out.matrix = *this;
    Matrix& A = out.matrix;
    std::size_t lead = 0;
    for (std::size_t r = 0; r < rows_ && lead < cols_; ++r) {
        // find pivot
        std::size_t best_row = r;
        double best = 0.0;
        while (lead < cols_) {
            best_row = r;
            best = abs(A(r, lead));
            for (std::size_t i = r + 1; i < rows_; ++i)
                if (abs(A(i, lead)) > best) { best = abs(A(i, lead)); best_row = i; }
            if (best > tol) break;
            for (std::size_t i = r; i < rows_; ++i) A(i, lead) = 0.0;
            ++lead;
        }
        if (lead >= cols_) break;
        A.swap_rows(r, best_row);
        const double pv = A(r, lead);
        for (std::size_t j = 0; j < cols_; ++j) A(r, j) /= pv;
        for (std::size_t i = 0; i < rows_; ++i) {
            if (i == r) continue;
            const double f = A(i, lead);
            if (f == 0.0) continue;
            for (std::size_t j = 0; j < cols_; ++j) A(i, j) -= f * A(r, j);
            A(i, lead) = 0.0;
        }
        out.pivots.push_back(lead);
        ++lead;
    }
    out.rank = out.pivots.size();
    // clean -0.0 and tiny noise
    for (double& v : A.data_)
        if (abs(v) < tol) v = 0.0;
    return out;
}

std::size_t Matrix::rank(double tol) const { return rref(tol).rank; }

double Matrix::determinant() const {
    check_square("determinant");
    if (rows_ == 1) return data_[0];
    if (rows_ == 2) return data_[0] * data_[3] - data_[1] * data_[2];
    const LU f = lu();
    if (f.singular) return 0.0;
    double det = static_cast<double>(f.sign);
    for (std::size_t i = 0; i < rows_; ++i) det *= f.U(i, i);
    return det;
}

Matrix Matrix::inverse() const {
    check_square("inverse");
    const std::size_t n = rows_;
    Matrix aug = hstack(identity(n));
    const RREF r = aug.rref(0.0);
    for (std::size_t i = 0; i < n; ++i)
        if (abs(r.matrix(i, i) - 1.0) > 1e-12) throw std::runtime_error("matrix is singular");
    return r.matrix.submatrix(0, n, n, n);
}

Matrix Matrix::cofactor_matrix() const {
    check_square("cofactor matrix");
    const std::size_t n = rows_;
    Matrix c(n, n, 0.0);
    if (n == 1) { c(0, 0) = 1.0; return c; }
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            const double m = minor_matrix(i, j).determinant();
            c(i, j) = ((i + j) % 2 == 0) ? m : -m;
        }
    return c;
}
Matrix Matrix::adjugate() const { return cofactor_matrix().transpose(); }

std::vector<double> Matrix::solve(const std::vector<double>& b) const {
    check_square("solve");
    if (b.size() != rows_) throw std::invalid_argument("right-hand side size does not match matrix");
    const Matrix X = solve(column(b));
    return X.col(0);
}

Matrix Matrix::solve(const Matrix& B) const {
    check_square("solve");
    if (B.rows_ != rows_) throw std::invalid_argument("right-hand side rows do not match matrix");
    const std::size_t n = rows_;
    const LU f = lu();
    if (f.singular) throw std::runtime_error("matrix is singular");
    for (std::size_t i = 0; i < n; ++i)
        if (f.U(i, i) == 0.0) throw std::runtime_error("matrix is singular");
    Matrix X(n, B.cols_, 0.0);
    std::vector<double> y(n), x(n);
    for (std::size_t c = 0; c < B.cols_; ++c) {
        // Pb
        for (std::size_t i = 0; i < n; ++i) y[i] = B(f.perm[i], c);
        // forward substitution L y = Pb
        for (std::size_t i = 0; i < n; ++i) {
            double s = y[i];
            for (std::size_t j = 0; j < i; ++j) s -= f.L(i, j) * y[j];
            y[i] = s;
        }
        // back substitution U x = y
        for (std::size_t i = n; i-- > 0;) {
            double s = y[i];
            for (std::size_t j = i + 1; j < n; ++j) s -= f.U(i, j) * x[j];
            x[i] = s / f.U(i, i);
        }
        for (std::size_t i = 0; i < n; ++i) X(i, c) = x[i];
    }
    return X;
}

std::vector<double> Matrix::solve_least_squares(const std::vector<double>& b) const {
    if (b.size() != rows_) throw std::invalid_argument("right-hand side size does not match matrix");
    if (rows_ < cols_) throw std::invalid_argument("least squares requires rows >= cols");
    const QR f = qr();
    // R x = Q^T b  (use the top cols_ rows)
    std::vector<double> qtb(rows_, 0.0);
    for (std::size_t i = 0; i < rows_; ++i) {
        double s = 0.0;
        for (std::size_t k = 0; k < rows_; ++k) s += f.Q(k, i) * b[k];
        qtb[i] = s;
    }
    std::vector<double> x(cols_, 0.0);
    for (std::size_t i = cols_; i-- > 0;) {
        double s = qtb[i];
        for (std::size_t j = i + 1; j < cols_; ++j) s -= f.R(i, j) * x[j];
        if (f.R(i, i) == 0.0) throw std::runtime_error("rank-deficient least squares system");
        x[i] = s / f.R(i, i);
    }
    return x;
}

double Matrix::condition_number() const {
    check_square("condition number");
    try {
        return norm_1() * inverse().norm_1();
    } catch (const std::runtime_error&) {
        return inf();
    }
}

Matrix::Eigen Matrix::eigen_symmetric(double tol, int max_sweeps) const {
    check_square("eigen decomposition");
    if (!is_symmetric(1e-9)) throw std::invalid_argument("eigen_symmetric requires a symmetric matrix");
    const std::size_t n = rows_;
    Matrix A = *this;
    Matrix V = identity(n);
    Eigen out;
    int sweeps = 0;
    for (; sweeps < max_sweeps; ++sweeps) {
        double off = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) off += A(i, j) * A(i, j);
        if (sqrt(off) < tol) { out.converged = true; break; }
        for (std::size_t p = 0; p < n; ++p) {
            for (std::size_t q = p + 1; q < n; ++q) {
                if (abs(A(p, q)) < 1e-300) continue;
                const double theta = (A(q, q) - A(p, p)) / (2.0 * A(p, q));
                const double t = (theta >= 0 ? 1.0 : -1.0) / (abs(theta) + sqrt(theta * theta + 1.0));
                const double c = 1.0 / sqrt(t * t + 1.0);
                const double s = t * c;
                for (std::size_t k = 0; k < n; ++k) {
                    const double akp = A(k, p), akq = A(k, q);
                    A(k, p) = c * akp - s * akq;
                    A(k, q) = s * akp + c * akq;
                }
                for (std::size_t k = 0; k < n; ++k) {
                    const double apk = A(p, k), aqk = A(q, k);
                    A(p, k) = c * apk - s * aqk;
                    A(q, k) = s * apk + c * aqk;
                }
                for (std::size_t k = 0; k < n; ++k) {
                    const double vkp = V(k, p), vkq = V(k, q);
                    V(k, p) = c * vkp - s * vkq;
                    V(k, q) = s * vkp + c * vkq;
                }
            }
        }
    }
    out.iterations = sweeps;
    // sort eigenvalues descending with vectors
    std::vector<std::size_t> idx(n);
    for (std::size_t i = 0; i < n; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return A(a, a) > A(b, b); });
    out.values.resize(n);
    out.vectors = Matrix(n, n, 0.0);
    for (std::size_t c = 0; c < n; ++c) {
        out.values[c] = A(idx[c], idx[c]);
        for (std::size_t r = 0; r < n; ++r) out.vectors(r, c) = V(r, idx[c]);
    }
    return out;
}

Matrix::Eigen Matrix::eigenvalues(double tol, int max_iter) const {
    check_square("eigenvalues");
    if (is_symmetric(1e-12)) {
        Eigen e = eigen_symmetric(tol);
        e.imag.assign(e.values.size(), 0.0);
        return e;
    }
    const std::size_t n = rows_;
    Matrix H = *this;
    // Reduce to upper Hessenberg form with Householder reflections.
    for (std::size_t k = 0; k + 2 < n; ++k) {
        double norm = 0.0;
        for (std::size_t i = k + 1; i < n; ++i) norm += H(i, k) * H(i, k);
        norm = sqrt(norm);
        if (norm == 0.0) continue;
        const double alpha = H(k + 1, k) > 0 ? -norm : norm;
        std::vector<double> v(n, 0.0);
        for (std::size_t i = k + 1; i < n; ++i) v[i] = H(i, k);
        v[k + 1] -= alpha;
        double vn = 0.0;
        for (std::size_t i = k + 1; i < n; ++i) vn += v[i] * v[i];
        if (vn == 0.0) continue;
        for (std::size_t j = 0; j < n; ++j) {
            double dot = 0.0;
            for (std::size_t i = k + 1; i < n; ++i) dot += v[i] * H(i, j);
            const double f = 2.0 * dot / vn;
            for (std::size_t i = k + 1; i < n; ++i) H(i, j) -= f * v[i];
        }
        for (std::size_t i = 0; i < n; ++i) {
            double dot = 0.0;
            for (std::size_t j = k + 1; j < n; ++j) dot += H(i, j) * v[j];
            const double f = 2.0 * dot / vn;
            for (std::size_t j = k + 1; j < n; ++j) H(i, j) -= f * v[j];
        }
    }
    // Shifted QR iteration with deflation (Francis single-shift via Wilkinson shift).
    Eigen out;
    std::size_t hi = n;
    int iter = 0;
    int stuck = 0;
    while (hi > 0 && iter < max_iter) {
        if (hi == 1) { out.values.push_back(H(0, 0)); out.imag.push_back(0.0); hi = 0; break; }
        // check for small sub-diagonal
        std::size_t l = hi - 1;
        while (l > 0) {
            const double s = abs(H(l - 1, l - 1)) + abs(H(l, l));
            if (abs(H(l, l - 1)) < tol * (s == 0.0 ? 1.0 : s)) break;
            --l;
        }
        if (l == hi - 1) {               // 1x1 block converged
            out.values.push_back(H(hi - 1, hi - 1));
            out.imag.push_back(0.0);
            --hi;
            stuck = 0;
            continue;
        }
        if (l == hi - 2) {               // 2x2 block: solve directly
            const double a = H(hi - 2, hi - 2), b = H(hi - 2, hi - 1), c = H(hi - 1, hi - 2), d = H(hi - 1, hi - 1);
            const double tr = a + d, det = a * d - b * c;
            const double disc = tr * tr / 4.0 - det;
            if (disc >= 0.0) {
                const double sq = sqrt(disc);
                out.values.push_back(tr / 2.0 + sq); out.imag.push_back(0.0);
                out.values.push_back(tr / 2.0 - sq); out.imag.push_back(0.0);
            } else {
                const double sq = sqrt(-disc);
                out.values.push_back(tr / 2.0); out.imag.push_back(sq);
                out.values.push_back(tr / 2.0); out.imag.push_back(-sq);
            }
            hi -= 2;
            stuck = 0;
            continue;
        }
        // Wilkinson shift from trailing 2x2 of the active block
        const double a = H(hi - 2, hi - 2), b = H(hi - 2, hi - 1), c = H(hi - 1, hi - 2), d = H(hi - 1, hi - 1);
        double mu = d;
        {
            const double tr = a + d, det = a * d - b * c;
            const double disc = tr * tr / 4.0 - det;
            if (disc >= 0.0) {
                const double sq = sqrt(disc);
                const double e1 = tr / 2.0 + sq, e2 = tr / 2.0 - sq;
                mu = (abs(e1 - d) < abs(e2 - d)) ? e1 : e2;
            }
        }
        if (++stuck % 11 == 0) mu += abs(H(hi - 1, hi - 2)); // exceptional shift
        // QR step on active block [l, hi) via Givens rotations
        const std::size_t m = hi - l;
        Matrix B = H.submatrix(l, l, m, m);
        for (std::size_t i = 0; i < m; ++i) B(i, i) -= mu;
        std::vector<double> cs(m - 1), sn(m - 1);
        for (std::size_t k = 0; k + 1 < m; ++k) {
            const double x = B(k, k), y = B(k + 1, k);
            const double r = hypot(x, y);
            const double cc = r == 0.0 ? 1.0 : x / r, ss = r == 0.0 ? 0.0 : y / r;
            cs[k] = cc; sn[k] = ss;
            for (std::size_t j = 0; j < m; ++j) {
                const double t1 = B(k, j), t2 = B(k + 1, j);
                B(k, j) = cc * t1 + ss * t2;
                B(k + 1, j) = -ss * t1 + cc * t2;
            }
        }
        for (std::size_t k = 0; k + 1 < m; ++k) {
            for (std::size_t i = 0; i < m; ++i) {
                const double t1 = B(i, k), t2 = B(i, k + 1);
                B(i, k) = cs[k] * t1 + sn[k] * t2;
                B(i, k + 1) = -sn[k] * t1 + cs[k] * t2;
            }
        }
        for (std::size_t i = 0; i < m; ++i) B(i, i) += mu;
        for (std::size_t i = 0; i < m; ++i)
            for (std::size_t j = 0; j < m; ++j) H(l + i, l + j) = B(i, j);
        ++iter;
    }
    out.iterations = iter;
    out.converged = (hi == 0);
    if (!out.converged) {
        for (std::size_t i = 0; i < hi; ++i) { out.values.push_back(H(i, i)); out.imag.push_back(0.0); }
    }
    // sort by real part descending then imag
    std::vector<std::size_t> idx(out.values.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [&](std::size_t x, std::size_t y) {
        if (out.values[x] != out.values[y]) return out.values[x] > out.values[y];
        return out.imag[x] > out.imag[y];
    });
    std::vector<double> v2, i2;
    for (std::size_t k : idx) { v2.push_back(out.values[k]); i2.push_back(out.imag[k]); }
    out.values = v2;
    out.imag = i2;
    return out;
}

std::pair<double, std::vector<double>> Matrix::power_iteration(int max_iter, double tol) const {
    check_square("power iteration");
    const std::size_t n = rows_;
    std::vector<double> v(n, 1.0);
    double lambda = 0.0;
    for (int it = 0; it < max_iter; ++it) {
        std::vector<double> w = (*this) * v;
        double norm = 0.0;
        for (double x : w) norm += x * x;
        norm = sqrt(norm);
        if (norm == 0.0) break;
        for (double& x : w) x /= norm;
        double num = 0.0, den = 0.0;
        const std::vector<double> Aw = (*this) * w;
        for (std::size_t i = 0; i < n; ++i) { num += w[i] * Aw[i]; den += w[i] * w[i]; }
        const double nl = num / den;
        double diff = 0.0;
        for (std::size_t i = 0; i < n; ++i) diff += (w[i] - v[i]) * (w[i] - v[i]);
        v = w;
        const bool done = abs(nl - lambda) < tol * fmax(1.0, abs(nl)) && sqrt(diff) < tol;
        lambda = nl;
        if (done) break;
    }
    return {lambda, v};
}

std::string Matrix::to_string(int precision) const {
    std::ostringstream os;
    os.precision(precision);
    for (std::size_t i = 0; i < rows_; ++i) {
        os << (i == 0 ? "[" : " ");
        for (std::size_t j = 0; j < cols_; ++j) {
            if (j) os << ", ";
            os << (*this)(i, j);
        }
        os << (i + 1 == rows_ ? "]" : ";\n");
    }
    if (rows_ == 0) os << "[]";
    return os.str();
}
bool Matrix::approx_equal(const Matrix& o, double tol) const {
    if (rows_ != o.rows_ || cols_ != o.cols_) return false;
    for (std::size_t i = 0; i < data_.size(); ++i)
        if (!nearly_equal(data_[i], o.data_[i], tol, tol)) return false;
    return true;
}

Matrix operator+(const Matrix& a, const Matrix& b) { Matrix r = a; r += b; return r; }
Matrix operator-(const Matrix& a, const Matrix& b) { Matrix r = a; r -= b; return r; }
Matrix operator*(const Matrix& a, const Matrix& b) { return a.multiply(b); }
Matrix operator*(const Matrix& a, double s) { Matrix r = a; r *= s; return r; }
Matrix operator*(double s, const Matrix& a) { Matrix r = a; r *= s; return r; }
Matrix operator/(const Matrix& a, double s) { Matrix r = a; r /= s; return r; }
std::vector<double> operator*(const Matrix& a, const std::vector<double>& v) {
    if (v.size() != a.cols()) throw std::invalid_argument("vector size does not match matrix columns");
    std::vector<double> r(a.rows(), 0.0);
    for (std::size_t i = 0; i < a.rows(); ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < a.cols(); ++j) s += a(i, j) * v[j];
        r[i] = s;
    }
    return r;
}
bool operator==(const Matrix& a, const Matrix& b) { return a.rows() == b.rows() && a.cols() == b.cols() && a.data() == b.data(); }
bool operator!=(const Matrix& a, const Matrix& b) { return !(a == b); }
std::ostream& operator<<(std::ostream& os, const Matrix& m) { return os << m.to_string(); }

} // namespace cml
