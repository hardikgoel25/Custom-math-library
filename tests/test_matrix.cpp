// tests/test_matrix.cpp
#include "cml_test.hpp"
#include "cml/matrix.hpp"

#include <cmath>
#include <stdexcept>

using cml::Matrix;

TEST_CASE("matrix: construction, access, shape errors") {
    Matrix a{{1, 2, 3}, {4, 5, 6}};
    CHECK_EQ(a.rows(), 2u);
    CHECK_EQ(a.cols(), 3u);
    CHECK_EQ(a(1, 2), 6.0);
    CHECK_THROWS(a(2, 0), std::out_of_range);
    CHECK_THROWS(a(0, 3), std::out_of_range);
    CHECK_THROWS((Matrix{{1, 2}, {3}}), std::invalid_argument);
    CHECK_EQ(Matrix::identity(3).trace(), 3.0);
    CHECK_EQ(Matrix::zeros(2, 2).sum(), 0.0);
    CHECK_EQ(Matrix::ones(2, 3).sum(), 6.0);
    CHECK_EQ(Matrix::diagonal({1, 2, 3}).determinant(), 6.0);
    CHECK_EQ(Matrix::parse("1 2; 3 4")(1, 0), 3.0);
    CHECK_EQ(Matrix::parse("[[1,2],[3,4]]")(0, 1), 2.0);
    CHECK_EQ(Matrix::parse("1,2\n3,4").rows(), 2u);
    CHECK_THROWS(Matrix::parse("1 2; 3"), std::invalid_argument);
    CHECK_THROWS(Matrix::parse("1 a"), std::invalid_argument);
    CHECK_THROWS(Matrix::parse(""), std::invalid_argument);
    CHECK_EQ(a.transpose()(2, 1), 6.0);
    CHECK_EQ(a.row(1).size(), 3u);
    CHECK_EQ(a.col(2)[1], 6.0);
    CHECK_EQ(a.submatrix(0, 1, 2, 2)(0, 0), 2.0);
    CHECK_EQ(a.minor_matrix(0, 0).rows(), 1u);
    CHECK_EQ(a.resized(3, 3)(2, 2), 0.0);
    CHECK_EQ(a.hstack(a).cols(), 6u);
    CHECK_EQ(a.vstack(a).rows(), 4u);
    CHECK_THROWS(a.hstack(Matrix::identity(3)), std::invalid_argument);
    CHECK_EQ(a.norm_inf(), 15.0);
    CHECK_EQ(a.norm_1(), 9.0);
    CHECK_CLOSE(a.norm_frobenius(), std::sqrt(91.0), 1e-15);
    CHECK_EQ(a.min(), 1.0);
    CHECK_EQ(a.max(), 6.0);
    CHECK(Matrix().empty());
    CHECK_THROWS(Matrix().determinant(), std::invalid_argument);
    // size cap guards against allocation bombs
    const std::size_t cap = Matrix::max_elements();
    Matrix::set_max_elements(100);
    CHECK_THROWS(Matrix(11, 10), std::length_error);
    CHECK_THROWS(Matrix(1u << 20, 1u << 20), std::length_error); // would overflow size_t on 32-bit
    CHECK_NOTHROW(Matrix(10, 10));
    Matrix::set_max_elements(cap);
}

TEST_CASE("matrix: arithmetic") {
    Matrix a{{1, 2}, {3, 4}}, b{{5, 6}, {7, 8}};
    CHECK_EQ((a + b)(1, 1), 12.0);
    CHECK_EQ((b - a)(0, 0), 4.0);
    CHECK_EQ((a * b)(0, 0), 19.0);
    CHECK_EQ((a * b)(1, 1), 50.0);
    CHECK_EQ((a * 2.0)(1, 0), 6.0);
    CHECK_EQ((2.0 * a)(1, 0), 6.0);
    CHECK_EQ((a / 2.0)(1, 1), 2.0);
    CHECK_EQ((-a)(0, 0), -1.0);
    CHECK_EQ(a.hadamard(b)(1, 1), 32.0);
    CHECK_THROWS(a + Matrix::identity(3), std::invalid_argument);
    CHECK_THROWS(a * Matrix::identity(3), std::invalid_argument);
    CHECK_EQ(a.pow(0), Matrix::identity(2));
    CHECK_EQ(a.pow(3), a * a * a);
    const std::vector<double> v = a * std::vector<double>{1.0, 1.0};
    CHECK_EQ(v[0], 3.0);
    CHECK_EQ(v[1], 7.0);
    // Strassen agrees with the naive product, including non-square and odd sizes
    Matrix m(37, 53, 0.0), n(53, 29, 0.0);
    for (std::size_t i = 0; i < 37; ++i) for (std::size_t j = 0; j < 53; ++j) m(i, j) = std::sin(i * 0.3 + j);
    for (std::size_t i = 0; i < 53; ++i) for (std::size_t j = 0; j < 29; ++j) n(i, j) = std::cos(i - j * 0.7);
    CHECK(m.multiply_strassen(n, 8).approx_equal(m.multiply(n), 1e-10));
    Matrix c = a;
    c += b; c -= b; c *= 2.0; c /= 2.0;
    CHECK_EQ(c, a);
    c.swap_rows(0, 1);
    CHECK_EQ(c(0, 0), 3.0);
    c.scale_row(0, 2.0);
    CHECK_EQ(c(0, 1), 8.0);
    c.add_row_multiple(1, 0, 1.0);
    CHECK_EQ(c(0, 0), 7.0);
    c.swap_cols(0, 1);
    CHECK_EQ(c(0, 0), 10.0);
    c.scale_col(0, 0.5);
    CHECK_EQ(c(0, 0), 5.0);
    c.add_col_multiple(1, 0, 1.0);
    CHECK_EQ(c(0, 0), 12.0);
    CHECK_THROWS(c.swap_rows(0, 5), std::out_of_range);
}

TEST_CASE("matrix: determinant, inverse, solve, LU") {
    Matrix a{{2, -1, 0}, {-1, 2, -1}, {0, -1, 2}};
    CHECK_CLOSE(a.determinant(), 4.0, 1e-14);
    Matrix inv = a.inverse();
    CHECK((a * inv).approx_equal(Matrix::identity(3), 1e-12));
    CHECK_CLOSE(inv(0, 0), 0.75, 1e-14);
    Matrix big(6, 6, 0.0);
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j) big(i, j) = 1.0 / (i + j + 1.0); // Hilbert
    CHECK((big * big.inverse()).approx_equal(Matrix::identity(6), 1e-6));
    CHECK(big.condition_number() > 1e6);
    CHECK_THROWS((Matrix{{1, 2}, {2, 4}}).inverse(), std::runtime_error);
    CHECK_EQ((Matrix{{1, 2}, {2, 4}}).determinant(), 0.0);
    CHECK_EQ((Matrix{{1, 2}, {2, 4}}).condition_number(), INFINITY);
    const std::vector<double> x = a.solve(std::vector<double>{1.0, 0.0, 1.0});
    CHECK_CLOSE(x[0], 1.0, 1e-14);
    CHECK_CLOSE(x[1], 1.0, 1e-14);
    CHECK_CLOSE(x[2], 1.0, 1e-14);
    CHECK_THROWS(a.solve(std::vector<double>{1.0, 2.0}), std::invalid_argument);
    CHECK_THROWS((Matrix{{1, 2}, {2, 4}}).solve(std::vector<double>{1.0, 2.0}), std::runtime_error);
    // PA = LU
    Matrix p{{0, 1, 2}, {3, 4, 5}, {6, 7, 9}};
    const Matrix::LU f = p.lu();
    CHECK(!f.singular);
    Matrix pa(3, 3, 0.0);
    for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j) pa(i, j) = p(f.perm[i], j);
    CHECK((f.L * f.U).approx_equal(pa, 1e-12));
    CHECK_CLOSE(p.determinant(), -3.0, 1e-12);
    CHECK_CLOSE(p.cofactor_matrix()(0, 0), 1.0, 1e-12);
    CHECK((p.adjugate() * (1.0 / p.determinant())).approx_equal(p.inverse(), 1e-12));
    // 4x4 with known determinant
    Matrix q{{1, 2, 3, 4}, {5, 6, 7, 8}, {2, 6, 4, 8}, {3, 1, 1, 2}};
    CHECK_CLOSE(q.determinant(), 72.0, 1e-12);
    // 1x1 and 2x2 paths
    CHECK_EQ((Matrix{{7}}).determinant(), 7.0);
    CHECK_EQ((Matrix{{7}}).inverse()(0, 0), 1.0 / 7.0);
    CHECK_EQ((Matrix{{1, 2}, {3, 4}}).determinant(), -2.0);
}

TEST_CASE("matrix: rank, rref, QR, Cholesky, least squares") {
    Matrix a{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    CHECK_EQ(a.rank(), 2u);
    CHECK_EQ(Matrix::identity(4).rank(), 4u);
    CHECK_EQ(Matrix::zeros(3, 3).rank(), 0u);
    const Matrix::RREF r = a.rref();
    CHECK_EQ(r.rank, 2u);
    CHECK_EQ(r.pivots.size(), 2u);
    CHECK_EQ(r.matrix(0, 0), 1.0);
    CHECK_EQ(r.matrix(0, 1), 0.0);
    CHECK_CLOSE(r.matrix(0, 2), -1.0, 1e-12);
    CHECK_CLOSE(r.matrix(1, 2), 2.0, 1e-12);
    CHECK_EQ(r.matrix(2, 2), 0.0);
    Matrix b{{12, -51, 4}, {6, 167, -68}, {-4, 24, -41}};
    const Matrix::QR qr = b.qr();
    CHECK((qr.Q * qr.R).approx_equal(b, 1e-10));
    CHECK((qr.Q.transpose() * qr.Q).approx_equal(Matrix::identity(3), 1e-12));
    for (std::size_t i = 1; i < 3; ++i) for (std::size_t j = 0; j < i; ++j) CHECK_CLOSE_ABS(qr.R(i, j), 0.0, 1e-12);
    Matrix tall{{1, 1}, {1, 2}, {1, 3}, {1, 4}};
    const Matrix::QR qr2 = tall.qr();
    CHECK((qr2.Q * qr2.R).approx_equal(tall, 1e-12));
    const std::vector<double> ls = tall.solve_least_squares({6, 5, 7, 10});
    CHECK_CLOSE(ls[0], 3.5, 1e-12);
    CHECK_CLOSE(ls[1], 1.4, 1e-12);
    Matrix spd{{4, 12, -16}, {12, 37, -43}, {-16, -43, 98}};
    const Matrix L = spd.cholesky();
    CHECK((L * L.transpose()).approx_equal(spd, 1e-12));
    CHECK_EQ(L(0, 0), 2.0);
    CHECK_EQ(L(1, 0), 6.0);
    CHECK_EQ(L(2, 2), 3.0);
    CHECK_THROWS((Matrix{{1, 2}, {2, 1}}).cholesky(), std::runtime_error);  // not positive definite
    CHECK_THROWS((Matrix{{1, 2}, {3, 4}}).cholesky(), std::runtime_error);  // not symmetric
    CHECK(spd.is_symmetric());
    CHECK(!a.is_symmetric());
}

TEST_CASE("matrix: eigenvalues") {
    Matrix s{{2, 1}, {1, 2}};
    const Matrix::Eigen e = s.eigen_symmetric();
    CHECK(e.converged);
    CHECK_CLOSE(e.values[0], 3.0, 1e-12);
    CHECK_CLOSE(e.values[1], 1.0, 1e-12);
    // eigenvectors satisfy A v = lambda v
    for (std::size_t k = 0; k < 2; ++k) {
        const std::vector<double> v = e.vectors.col(k);
        const std::vector<double> av = s * v;
        for (std::size_t i = 0; i < 2; ++i) CHECK_CLOSE_ABS(av[i], e.values[k] * v[i], 1e-12);
    }
    Matrix s3{{4, 1, 2}, {1, 3, 0}, {2, 0, 5}};
    const Matrix::Eigen e3 = s3.eigen_symmetric();
    double tr = 0.0;
    for (double v : e3.values) tr += v;
    CHECK_CLOSE(tr, 12.0, 1e-12);
    CHECK((e3.vectors * Matrix::diagonal(e3.values) * e3.vectors.transpose()).approx_equal(s3, 1e-10));
    CHECK_THROWS((Matrix{{1, 2}, {3, 4}}).eigen_symmetric(), std::invalid_argument);
    // general matrix with complex eigenvalues (rotation by 90 degrees)
    const Matrix::Eigen rot = (Matrix{{0, -1}, {1, 0}}).eigenvalues();
    CHECK(rot.converged);
    CHECK_CLOSE_ABS(rot.values[0], 0.0, 1e-12);
    CHECK_CLOSE_ABS(std::fabs(rot.imag[0]), 1.0, 1e-12);
    CHECK_CLOSE_ABS(rot.imag[0] + rot.imag[1], 0.0, 1e-12);
    // general non-symmetric with real eigenvalues 1, 2, 3
    const Matrix::Eigen g = (Matrix{{2, 0, 0}, {1, 1, 0}, {3, 4, 3}}).eigenvalues();
    CHECK(g.converged);
    CHECK_CLOSE(g.values[0], 3.0, 1e-10);
    CHECK_CLOSE(g.values[1], 2.0, 1e-10);
    CHECK_CLOSE(g.values[2], 1.0, 1e-10);
    const Matrix::Eigen g2 = (Matrix{{4, 1, 2, 3}, {0, 3, 1, 2}, {1, 0, 2, 1}, {2, 1, 0, 1}}).eigenvalues();
    CHECK(g2.converged);
    double sum = 0.0;
    for (double v : g2.values) sum += v;
    CHECK_CLOSE(sum, 10.0, 1e-9);                                // trace
    const Matrix::Eigen sym_as_general = s3.eigenvalues();
    CHECK_CLOSE(sym_as_general.values[0], e3.values[0], 1e-12);
    const auto pi = s.power_iteration();
    CHECK_CLOSE(pi.first, 3.0, 1e-10);
    CHECK_CLOSE(std::fabs(pi.second[0]), std::sqrt(0.5), 1e-6);
}
