
#pragma once
#include <complex>
#include <string>

#ifndef __EMSCRIPTEN__
#error "This should be compiled as JS!"
#endif
#include <emscripten.h>

#include <Eigen/Dense>

using Num = std::complex<double>;
using CVector = Eigen::VectorXcd;
using CMatrix = Eigen::MatrixXcd;
using Vector = Eigen::VectorXd;

constexpr auto I = Num(0.0, 1.0);

using JsResult = int;

#define JS_RC_SUCCEEDED(expr) ((expr) == 0)

inline double NormSquared(const Num num) {
    return pow(num.real(), 2) + pow(num.imag(), 2);
}

inline constexpr Num Conjugate(const Num num) {
    return Num(num.real(), -num.imag());
}

inline CVector ConjugatedCVector(const CVector &vec) {
    CVector new_vec = CVector::Zero(vec.size());
    for(long i = 0; i < new_vec.size(); i++) {
        new_vec(i) = Conjugate(vec(i));
    }
    return new_vec;
}

inline Vector NormSquaredVector(const CVector &vec) {
    Vector new_vec = Vector::Zero(vec.size());
    for(long i = 0; i < new_vec.size(); i++) {
        new_vec(i) = NormSquared(vec(i));
    }
    return new_vec;
}

// Note: for this simulation's sake, just suppose post-extreme values are zero in order to estimate derivatives without losing vector points
// (since these are just applied to psi, they are actually taken as zero in simulation iterations)

template<typename V>
inline V VectorDerivative(const V &vec, const double dv) {
    V new_vec = V::Zero(vec.size());
    for(long i = 0; i < new_vec.size() - 1; i++) {
        new_vec(i) = (vec(i + 1) - vec(i)) / dv;
    }
    new_vec(new_vec.size() - 1) = (0.0 - vec(new_vec.size() - 1)) / dv;
    return new_vec;
}

template<typename V>
inline V VectorDDerivative(const V &vec, const double dv) {
    const auto dvsq = pow(dv, 2);
    V new_vec = V::Zero(vec.size());
    new_vec(0) = (vec(1) - 2.0 * vec(0)) / dvsq;
    for(long i = 1; i < new_vec.size() - 1; i++) {
        new_vec(i) = (vec(i + 1) - 2.0 * vec(i) + vec(i - 1)) / dvsq;
    }
    new_vec(new_vec.size() - 1) = (- 2.0 * vec(new_vec.size() - 1) + vec(new_vec.size() - 2)) / dvsq;
    return new_vec;
}
