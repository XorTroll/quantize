#include "js_export.hpp"

EM_JS(double, gauss, (const double x, const double x0, const double k0, const double a), {
    return math.multiply(math.nthRoot(2.0 / (math.PI * a**2), 4), math.exp(math.complex(0, k0 * (x - x0))), math.exp(- (((x - x0)/a)**2)));
});

// Defined in main, where the quantum simulator is instantiated

extern "C" __attribute__((used)) double cpp_ApproximateDiracDelta(const double x, const double x0, const double val);

EM_JS(double, delta, (const double x, const double x0, const double val), {
    return Module.ccall("cpp_ApproximateDiracDelta", "number", ["number", "number", "number"], [x, x0, val]);
});

namespace {

    using CoefficientList = std::vector<double>;

    void DerivatePolynomial(CoefficientList &poly) {
        poly.erase(poly.begin());
        for(int i = 0; i < poly.size(); i++) {
            poly.at(i) *= (i + 1);
        }
    }

    void PolynomialTimesConstant(CoefficientList &poly, const double c) {
        for(int i = 0; i < poly.size(); i++) {
            poly.at(i) *= c;
        }
    }
    
    void PolynomialTimesX(CoefficientList &poly) {
        poly.insert(poly.begin(), 0.0);
    }
    
    void AddPolynomials(CoefficientList &base, CoefficientList &add) {
        while(add.size() > base.size()) {
            base.push_back(0.0);
        }

        for(int i = 0; i < add.size(); i++) {
            base.at(i) += add.at(i);
        }
    }

    double EvaluatePolynomial(const CoefficientList &poly, const double x) {
        double val = 0.0;
        for(int i = 0; i < poly.size(); i++) {
            val += poly.at(i) * pow(x, i);
        }
        return val;
    }

    CoefficientList HermitePolynomial(const int n) {
        if(n == 0) {
            return { 1.0 };
        }
        else {
            // H_{n} = 2*x*H_{n-1} - H_{n-1}'

            auto poly_nm1_a = HermitePolynomial(n - 1);
            PolynomialTimesX(poly_nm1_a);
            PolynomialTimesConstant(poly_nm1_a, 2.0);

            auto poly_nm1_b = HermitePolynomial(n - 1);
            DerivatePolynomial(poly_nm1_b);
            PolynomialTimesConstant(poly_nm1_b, -1.0);

            AddPolynomials(poly_nm1_a, poly_nm1_b);

            return poly_nm1_a;
        }
    }

}

extern "C" EMSCRIPTEN_KEEPALIVE double cpp_Hermite(const int n, const double x) {
    const auto h = HermitePolynomial(n);
    return EvaluatePolynomial(h, x);
}

EM_JS(double, hermite, (const int n, const double x), {
    return Module.ccall("cpp_Hermite", "number", ["number", "number"], [n, x]);
});

void InitializeJsExports() {
    // Invoke the functions so that clang finds them used in C++
    gauss(0, 0, 0, 0);
    delta(0, 0, 0);
    hermite(0, 0);
}
