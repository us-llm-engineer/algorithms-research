#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algo {

class Simplex {
 public:
  using Vector = std::vector<double>;
  using Matrix = std::vector<Vector>;

  enum class Status { Optimal, Infeasible, Unbounded };

  struct Result {
    Status status = Status::Infeasible;
    Vector x;
    double objective = 0.0;
    Vector dual;

    // Retained so the public validators check the actual LP that was solved.
    Matrix constraints;
    Vector rhs;
    Vector coefficients;
  };

  static Result solve(const Matrix& a, const Vector& b, const Vector& c) {
    if (a.size() != b.size()) throw std::invalid_argument("constraint and rhs counts differ");
    const std::size_t n = c.size();
    for (const auto& row : a) {
      if (row.size() != n) throw std::invalid_argument("constraint row has wrong width");
      for (double value : row) require_finite(value);
    }
    for (double value : b) require_finite(value);
    for (double value : c) require_finite(value);

    Result out;
    out.constraints = a;
    out.rhs = b;
    out.coefficients = c;
    out.x.assign(n, 0.0);
    out.dual.assign(b.size(), 0.0);

    // With no decision variables, the only possible point is the empty vector.
    if (n == 0) {
      for (double bound : b) {
        if (bound < -kEps) {
          out.status = Status::Infeasible;
          return out;
        }
      }
      out.status = Status::Optimal;
      return out;
    }

    Tableau solver(a, b, c);
    Vector solution;
    double optimum = 0.0;
    const auto state = solver.run(solution, optimum);
    if (state == Tableau::State::Infeasible) {
      out.status = Status::Infeasible;
      return out;
    }
    if (state == Tableau::State::Unbounded) {
      out.status = Status::Unbounded;
      return out;
    }
    out.status = Status::Optimal;
    out.x = std::move(solution);
    out.objective = optimum;
    out.dual = solver.dual_solution();
    return out;
  }

  static bool feasible(const Result& result, double tolerance = 1e-9) {
    if (result.status != Status::Optimal || tolerance < 0.0 ||
        result.x.size() != result.coefficients.size() ||
        result.constraints.size() != result.rhs.size()) return false;
    for (double x : result.x) if (!std::isfinite(x) || x < -tolerance) return false;
    for (std::size_t i = 0; i < result.constraints.size(); ++i) {
      if (result.constraints[i].size() != result.x.size()) return false;
      double lhs = 0.0;
      for (std::size_t j = 0; j < result.x.size(); ++j)
        lhs += result.constraints[i][j] * result.x[j];
      if (lhs > result.rhs[i] + tolerance * (1.0 + std::abs(result.rhs[i]))) return false;
    }
    return true;
  }

  static bool certificate_valid(const Result& result, double tolerance = 1e-8) {
    if (!feasible(result, tolerance) || result.dual.size() != result.rhs.size() ||
        !std::isfinite(result.objective)) return false;
    double dual_objective = 0.0;
    for (std::size_t i = 0; i < result.dual.size(); ++i) {
      const double y = result.dual[i];
      if (!std::isfinite(y) || y < -tolerance) return false;
      dual_objective += y * result.rhs[i];
    }
    for (std::size_t j = 0; j < result.coefficients.size(); ++j) {
      double bound = 0.0;
      for (std::size_t i = 0; i < result.dual.size(); ++i)
        bound += result.dual[i] * result.constraints[i][j];
      if (bound + tolerance * (1.0 + std::abs(result.coefficients[j])) < result.coefficients[j]) return false;
    }
    const double scale = 1.0 + std::abs(result.objective) + std::abs(dual_objective);
    return std::abs(dual_objective - result.objective) <= tolerance * scale;
  }

 private:
  static constexpr double kEps = 1e-9;

  static void require_finite(double value) {
    if (!std::isfinite(value)) throw std::invalid_argument("LP coefficients must be finite");
  }

  class Tableau {
   public:
    enum class State { Optimal, Infeasible, Unbounded };

    Tableau(const Matrix& a, const Vector& b, const Vector& c)
        : m_(static_cast<int>(b.size())), n_(static_cast<int>(c.size())),
          basis_(static_cast<std::size_t>(m_)), nonbasis_(static_cast<std::size_t>(n_ + 1)),
          d_(static_cast<std::size_t>(m_ + 2),
             Vector(static_cast<std::size_t>(n_ + 2), 0.0)), a_(a), c_(c), b_(b) {
      for (int i = 0; i < m_; ++i) {
        for (int j = 0; j < n_; ++j)
          d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
              a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        basis_[static_cast<std::size_t>(i)] = n_ + i;
        d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(n_)] = -1.0;
        d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(n_ + 1)] = b[static_cast<std::size_t>(i)];
      }
      for (int j = 0; j < n_; ++j) {
        nonbasis_[static_cast<std::size_t>(j)] = j;
        d_[static_cast<std::size_t>(m_)][static_cast<std::size_t>(j)] = -c[static_cast<std::size_t>(j)];
      }
      nonbasis_[static_cast<std::size_t>(n_)] = -1;
      d_[static_cast<std::size_t>(m_ + 1)][static_cast<std::size_t>(n_)] = 1.0;
    }

    State run(Vector& x, double& optimum) {
      if (m_ > 0) {
        int r = 0;
        for (int i = 1; i < m_; ++i)
          if (d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(n_ + 1)] <
              d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(n_ + 1)]) r = i;
        if (d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(n_ + 1)] < -kEps) {
          pivot(r, n_);
          if (!simplex(1) || d_[static_cast<std::size_t>(m_ + 1)][static_cast<std::size_t>(n_ + 1)] < -kEps)
            return State::Infeasible;
          if (std::abs(d_[static_cast<std::size_t>(m_ + 1)][static_cast<std::size_t>(n_ + 1)]) > kEps)
            return State::Infeasible;
          for (int i = 0; i < m_; ++i) {
            if (basis_[static_cast<std::size_t>(i)] == -1) {
              int s = 0;
              for (int j = 1; j <= n_; ++j) {
                if (std::abs(d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]) >
                    std::abs(d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)]) + kEps ||
                    (std::abs(std::abs(d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]) -
                              std::abs(d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)])) <= kEps &&
                     nonbasis_[static_cast<std::size_t>(j)] < nonbasis_[static_cast<std::size_t>(s)])) s = j;
              }
              pivot(i, s);
            }
          }
        }
      }
      if (!simplex(2)) return State::Unbounded;
      x.assign(static_cast<std::size_t>(n_), 0.0);
      for (int i = 0; i < m_; ++i)
        if (basis_[static_cast<std::size_t>(i)] >= 0 && basis_[static_cast<std::size_t>(i)] < n_)
          x[static_cast<std::size_t>(basis_[static_cast<std::size_t>(i)])] =
              d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(n_ + 1)];
      optimum = d_[static_cast<std::size_t>(m_)][static_cast<std::size_t>(n_ + 1)];
      return State::Optimal;
    }

    Vector dual_solution() const {
      // Recover y from B^T y = c_B. Slack variables identify the original rows.
      Vector matrix(static_cast<std::size_t>(m_ * (m_ + 1)), 0.0);
      auto at = [&](int row, int col) -> double& {
        return matrix[static_cast<std::size_t>(row * (m_ + 1) + col)];
      };
      for (int equation = 0; equation < m_; ++equation) {
        for (int basis_row = 0; basis_row < m_; ++basis_row) {
          int var = basis_[static_cast<std::size_t>(basis_row)];
          double value = 0.0;
          if (var >= 0 && var < n_) value = a_[static_cast<std::size_t>(equation)][static_cast<std::size_t>(var)];
          else if (var >= n_ && var < n_ + m_ && var - n_ == equation) value = 1.0;
          at(basis_row, equation) = value;
        }
        int var = basis_[static_cast<std::size_t>(equation)];
        at(equation, m_) = (var >= 0 && var < n_) ? c_[static_cast<std::size_t>(var)] : 0.0;
      }
      // Gaussian elimination with partial pivoting for the transposed basis.
      for (int col = 0; col < m_; ++col) {
        int pivot_row = col;
        for (int r = col + 1; r < m_; ++r)
          if (std::abs(at(r, col)) > std::abs(at(pivot_row, col))) pivot_row = r;
        if (std::abs(at(pivot_row, col)) <= kEps) return Vector(static_cast<std::size_t>(m_), 0.0);
        for (int j = col; j <= m_; ++j) std::swap(at(col, j), at(pivot_row, j));
        const double divisor = at(col, col);
        for (int j = col; j <= m_; ++j) at(col, j) /= divisor;
        for (int r = 0; r < m_; ++r) if (r != col) {
          const double factor = at(r, col);
          for (int j = col; j <= m_; ++j) at(r, j) -= factor * at(col, j);
        }
      }
      Vector y(static_cast<std::size_t>(m_));
      for (int i = 0; i < m_; ++i) y[static_cast<std::size_t>(i)] = at(i, m_);
      return y;
    }

   private:
    int m_, n_;
    std::vector<int> basis_, nonbasis_;
    Matrix d_;
    const Matrix& a_;
    const Vector& c_;
    const Vector& b_;

    void pivot(int r, int s) {
      const double inverse = 1.0 / d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(s)];
      for (int i = 0; i < m_ + 2; ++i) if (i != r)
        for (int j = 0; j < n_ + 2; ++j) if (j != s)
          d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] -=
              d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(j)] *
              d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] * inverse;
      for (int j = 0; j < n_ + 2; ++j) if (j != s)
        d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(j)] *= inverse;
      for (int i = 0; i < m_ + 2; ++i) if (i != r)
        d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] *= -inverse;
      d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(s)] = inverse;
      std::swap(basis_[static_cast<std::size_t>(r)], nonbasis_[static_cast<std::size_t>(s)]);
    }

    bool simplex(int phase) {
      const int objective_row = (phase == 1) ? m_ + 1 : m_;
      for (;;) {
        int s = -1;
        for (int j = 0; j <= n_; ++j) {
          if (phase == 2 && nonbasis_[static_cast<std::size_t>(j)] == -1) continue;
          if (s == -1 || d_[static_cast<std::size_t>(objective_row)][static_cast<std::size_t>(j)] <
                             d_[static_cast<std::size_t>(objective_row)][static_cast<std::size_t>(s)] - kEps ||
              (std::abs(d_[static_cast<std::size_t>(objective_row)][static_cast<std::size_t>(j)] -
                        d_[static_cast<std::size_t>(objective_row)][static_cast<std::size_t>(s)]) <= kEps &&
               nonbasis_[static_cast<std::size_t>(j)] < nonbasis_[static_cast<std::size_t>(s)])) s = j;
        }
        if (s == -1 || d_[static_cast<std::size_t>(objective_row)][static_cast<std::size_t>(s)] >= -kEps) return true;
        int r = -1;
        for (int i = 0; i < m_; ++i) {
          if (d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)] <= kEps) continue;
          if (r == -1) r = i;
          else {
            const double lhs = d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(n_ + 1)] /
                               d_[static_cast<std::size_t>(i)][static_cast<std::size_t>(s)];
            const double rhs = d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(n_ + 1)] /
                               d_[static_cast<std::size_t>(r)][static_cast<std::size_t>(s)];
            if (lhs < rhs - kEps || (std::abs(lhs - rhs) <= kEps && basis_[static_cast<std::size_t>(i)] < basis_[static_cast<std::size_t>(r)])) r = i;
          }
        }
        if (r == -1) return false;
        pivot(r, s);
      }
    }
  };
};

}  // namespace algo
