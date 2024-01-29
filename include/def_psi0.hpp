
#pragma once
#include <iterator>

namespace def_psi0 {

    constexpr const char SquarePulsePsi0[] =
        "// Square pulse spanning (xa, xb) space interval\n"
        "xa = -0.5;\n"
        "xb = 0.5;\n"
        "function psi0(x) {\n"
        "    if((x > xa) && (x < xb)) {\n"
        "        // Although not a hard requisite, this factor ensures a normalized state\n"
        "        return math.sqrt(1/(xb-xa));\n"
        "    }\n"
        "    else {\n"
        "        return 0;\n"
        "    }\n"
        "}";

    constexpr const char InfiniteWellPsi0[] =
        "// Infinite well of size 'u', eigenstate |n>\n"
        "n = 1;\n"
        "u = 0.5;\n"
        "\n"
        "function psi0(x) {\n"
        "    if((x > 0) && (x < u)) {\n"
        "        return math.sqrt(2/u) * math.sin((n*math.PI/u) * x);\n"
        "    }\n"
        "    else {\n"
        "        return 0;\n"
        "    }\n"
        "}";

    constexpr const char GaussianPacketPsi0[] =
        "// Gaussian wave packet\n"
        "k = 1; // Wavenumber (proportional to group velocity, aka packet center's speed)\n"
        "xc = 0; // Initial packet center position\n"
        "s = 0.25; // Width value (related to std. deviation)\n"
        "\n"
        "function psi0(x) {\n"
        "    return gauss(x, xc, k, s);\n"
        "}";

    constexpr const char HarmonicOscillatorPsi0[] =
        "// Harmonic oscillator eigenstate\n"
        "omega = 5; // Frequency\n"
        "n = 1; // Eigenstate |n>\n"
        "\n"
        "function psi0(x) {\n"
        "    return (1/math.sqrt((2**n)*math.factorial(n))) * math.nthRoot(((m*omega)/(math.PI*hslash)), 4) * math.exp(- ((m*omega)/(2*hslash)) * x**2) * hermite(n, math.sqrt((m*omega)/hslash) * x);\n"
        "}";

    constexpr const char DiracDeltaPsi0[] =
        "// Dirac delta at 'xc' (aka particle completely located there)\n"
        "xc = 0;\n"
        "\n"
        "function psi0(x) {\n"
        "    return delta(x, xc, 10000);\n"
        "}";

}

constexpr const char *Psi0DemoSources[] = {
    def_psi0::SquarePulsePsi0,
    def_psi0::InfiniteWellPsi0,
    def_psi0::GaussianPacketPsi0,
    def_psi0::HarmonicOscillatorPsi0,
    def_psi0::DiracDeltaPsi0
};

constexpr const char *Psi0DemoSourceNames[] = {
    "Square pulse",
    "Infinite well eigenstate",
    "Gaussian wave packet",
    "Harmonic oscillator eigenstate",
    "Dirac delta (position eigenstate)"
};

constexpr size_t Psi0DemoSourceCount = std::size(Psi0DemoSources);

constexpr auto DefaultPsi0Source = def_psi0::GaussianPacketPsi0;
