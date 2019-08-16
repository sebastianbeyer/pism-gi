/* Copyright (C) 2015, 2016, 2017, 2018, 2019 PISM Authors
 *
 * This file is part of PISM.
 *
 * PISM is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * PISM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PISM; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gsl/gsl_interp.h>
#include <cassert>

#include "interpolation.hh"
#include "error_handling.hh"

namespace pism {

Interpolation::Interpolation() {
  // empty
}

Interpolation::~Interpolation() {
  // empty
}

/**
 * Compute linear interpolation indexes and weights.
 *
 * @param[in] input_x coordinates of the input grid
 * @param[in] input_x_size number of points in the input grid
 * @param[in] output_x coordinates of the output grid
 * @param[in] output_x_size number of points in the output grid
 */
void Interpolation::init_linear(const double *input_x, unsigned int input_x_size,
                                const double *output_x, unsigned int output_x_size) {

  m_left.resize(output_x_size);
  m_right.resize(output_x_size);
  m_alpha.resize(output_x_size);

  // the trivial case
  if (input_x_size < 2) {
    m_left[0]  = 0;
    m_right[0] = 0;
    m_alpha[0] = 0.0;
    return;
  }

  // input grid points have to be stored in the increasing order
  for (unsigned int i = 0; i < input_x_size - 1; ++i) {
    if (input_x[i] >= input_x[i + 1]) {
      throw RuntimeError(PISM_ERROR_LOCATION, "an input grid for linear interpolation has to be "
                         "strictly increasing");
    }
  }

  // compute indexes and weights
  for (unsigned int i = 0; i < output_x_size; ++i) {
    double x = output_x[i];

    // gsl_interp_bsearch always returns an index "L" such that "L + 1" is valid
    unsigned int L = gsl_interp_bsearch(input_x, x, 0, input_x_size - 1);

    unsigned int R = x > input_x[L] ? L + 1 : L;

    m_left[i] = L;
    m_right[i] = R;

    if (L != R) {        // protect from division by zero
      if (x <= input_x[R]) {
        // regular case
        m_alpha[i] = (x - input_x[L]) / (input_x[R] - input_x[L]);
      } else {
        // extrapolation on the right
        m_alpha[i] = 1.0;
      }
    } else {
      // this corresponds to extrapolation on the left
      m_alpha[i] = 0.0;
    }

    assert(m_left[i] >= 0 and m_left[i] < (int)input_x_size);
    assert(m_right[i] >= 0 and m_right[i] < (int)input_x_size);
    assert(m_alpha[i] >= 0.0 and m_alpha[i] <= 1.0);
  }
}

const std::vector<int>& Interpolation::left() const {
  return m_left;
}

const std::vector<int>& Interpolation::right() const {
  return m_right;
}

const std::vector<double>& Interpolation::alpha() const {
  return m_alpha;
}

int Interpolation::left(size_t j) const {
  return m_left[j];
}

int Interpolation::right(size_t j) const {
  return m_right[j];
}

double Interpolation::alpha(size_t j) const {
  return m_alpha[j];
}

std::vector<double> Interpolation::interpolate(const std::vector<double> &input_values) const {
  std::vector<double> result(m_alpha.size());

  for (size_t k = 0; k < result.size(); ++k) {
    const int
      L = m_left[k],
      R = m_right[k];
    const double Alpha = m_alpha[k];
    result[k] = input_values[L] + Alpha * (input_values[R] - input_values[L]);
  }
  return result;
}

LinearInterpolation::LinearInterpolation(const std::vector<double> &input_x,
                                         const std::vector<double> &output_x) {

  this->init_linear(&input_x[0], input_x.size(), &output_x[0], output_x.size());
}

LinearInterpolation::LinearInterpolation(const double *input_x, unsigned int input_x_size,
                                         const double *output_x, unsigned int output_x_size) {
  this->init_linear(input_x, input_x_size, output_x, output_x_size);
}

NearestNeighbor::NearestNeighbor(const std::vector<double> &input_x,
                                 const std::vector<double> &output_x) {

  this->init(&input_x[0], input_x.size(), &output_x[0], output_x.size());
}

NearestNeighbor::NearestNeighbor(const double *input_x, unsigned int input_x_size,
                                 const double *output_x, unsigned int output_x_size) {
  this->init(input_x, input_x_size, output_x, output_x_size);
}

void NearestNeighbor::init(const double *input_x, unsigned int input_x_size,
                           const double *output_x, unsigned int output_x_size) {

  init_linear(input_x, input_x_size, output_x, output_x_size);

  for (unsigned int j = 0; j < m_alpha.size(); ++j) {
    m_alpha[j] = m_alpha[j] > 0.5 ? 1.0 : 0.0;
  }
}

PiecewiseConstant::PiecewiseConstant(const std::vector<double>& input_x,
                                     const std::vector<double>& output_x) {
  init(&input_x[0], input_x.size(), &output_x[0], output_x.size());
}

PiecewiseConstant::PiecewiseConstant(const double *input_x, unsigned int input_x_size,
                                     const double *output_x, unsigned int output_x_size) {
  init(input_x, input_x_size, output_x, output_x_size);
}

void PiecewiseConstant::init(const double *input_x, unsigned int input_x_size,
                             const double *output_x, unsigned int output_x_size) {

  m_left.resize(output_x_size);
  m_right.resize(output_x_size);
  m_alpha.resize(output_x_size);

  // the trivial case
  if (input_x_size < 2) {
    m_left[0]  = 0;
    m_right[0] = 0;
    m_alpha[0] = 0.0;
    return;
  }

  // input grid points have to be stored in the increasing order
  for (unsigned int i = 0; i < input_x_size - 1; ++i) {
    if (input_x[i] >= input_x[i + 1]) {
      throw RuntimeError(PISM_ERROR_LOCATION, "an input grid for interpolation has to be "
                         "strictly increasing");
    }
  }

  gsl_interp_accel *accel = gsl_interp_accel_alloc();
  if (accel == NULL) {
    throw RuntimeError(PISM_ERROR_LOCATION,
                       "Failed to allocate a GSL interpolation accelerator");
  }

  try {
    // compute indexes and weights
    for (unsigned int i = 0; i < output_x_size; ++i) {
      double x = output_x[i];

      // gsl_interp_bsearch always returns an index "L" such that "L + 1" is valid
      unsigned int L = gsl_interp_bsearch(input_x, x, 0, input_x_size - 1);

      m_left[i] = L;
      m_right[i] = L;
      m_alpha[i] = 0.0;

      assert(m_left[i] >= 0 and m_left[i] < (int)input_x_size);
      assert(m_right[i] >= 0 and m_right[i] < (int)input_x_size);
      assert(m_alpha[i] >= 0.0 and m_alpha[i] <= 1.0);
    }
  } catch (...) {
    gsl_interp_accel_free(accel);
    accel = nullptr;
  }

  if (accel != nullptr) {
    gsl_interp_accel_free(accel);
  }
}

LinearPeriodic::LinearPeriodic(const std::vector<double>& input_x,
                               const std::vector<double>& output_x,
                               double period) {
  init(&input_x[0], input_x.size(), &output_x[0], output_x.size(), period);
}

LinearPeriodic::LinearPeriodic(const double *input_x, unsigned int input_x_size,
                               const double *output_x, unsigned int output_x_size,
                               double period) {
  init(input_x, input_x_size, output_x, output_x_size, period);
}

void LinearPeriodic::init(const double *input_x, unsigned int input_x_size,
                          const double *output_x, unsigned int output_x_size,
                          double period) {

  m_left.resize(output_x_size);
  m_right.resize(output_x_size);
  m_alpha.resize(output_x_size);

  // the trivial case
  if (input_x_size < 2) {
    m_left[0]  = 0;
    m_right[0] = 0;
    m_alpha[0] = 0.0;
    return;
  }

  // input grid points have to be stored in the increasing order
  for (unsigned int i = 0; i < input_x_size - 1; ++i) {
    if (input_x[i] >= input_x[i + 1]) {
      throw RuntimeError(PISM_ERROR_LOCATION, "an input grid for interpolation has to be "
                         "strictly increasing");
    }
  }

  gsl_interp_accel *accel = gsl_interp_accel_alloc();
  if (accel == NULL) {
    throw RuntimeError(PISM_ERROR_LOCATION,
                       "Failed to allocate a GSL interpolation accelerator");
  }

  try {
    // compute indexes and weights
    for (unsigned int i = 0; i < output_x_size; ++i) {
      double x = output_x[i];

      unsigned int L = 0, R = 0;
      if (x < input_x[0]) {
        L = input_x_size - 1;
        R = 0.0;
      } else {
        L = gsl_interp_bsearch(input_x, x, 0, input_x_size);
        R = L + 1 < input_x_size ? L + 1 : 0;
      }

      double
        x_l = input_x[L],
        x_r = input_x[R],
        alpha = 0.0;
      if (L < R) {
        alpha = (x - x_l) / (x_r - x_l);
      } else {
        double
          x0 = input_x[0],
          dx = (period - x_l) + x0;
        if (x > x0) {
          alpha = (x - x_l) / dx;
        } else {
          alpha = 1.0 - (x_r - x) / dx;
        }
      }

      assert(L >= 0 and L < input_x_size);
      assert(R >= 0 and R < input_x_size);
      assert(alpha >= 0.0 and alpha <= 1.0);

      m_left[i] = L;
      m_right[i] = R;
      m_alpha[i] = alpha;
    }
  } catch (...) {
    gsl_interp_accel_free(accel);
    accel = nullptr;
  }

  if (accel != nullptr) {
    gsl_interp_accel_free(accel);
  }
}

} // end of namespace pism
