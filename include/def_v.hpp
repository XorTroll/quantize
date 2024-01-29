
#pragma once
#include <iterator>

namespace def_v {

    constexpr const char NoneV[] =
        "// Free particle, no potential\n"
        "function V(x, t) {\n"
        "    return 0;\n"
        "}";

    constexpr const char InfiniteWellV[] =
        "// Well of size 'w'\n"
        "w = 1;\n"
        "\n"
        "function V(x, t) {\n"
        "    if((x > -w/2) && (x < w/2)) {\n"
        "        return 0;\n"
        "    }\n"
        "    else {\n"
        "        return 10000;\n"
        "    }\n"
        "}";

    constexpr const char DiracDeltaV[] =
        "// Dirac delta at 'xc'\n"
        "xc = 0;\n"
        "\n"
        "function V(x, t) {\n"
        "    return delta(x, xc, 10000);\n"
        "}";

    constexpr const char StepV[] =
        "// Finite step potential\n"
        "V0 = 2; // Step height\n"
        "si = -0.5; // Step start position\n"
        "sf = 0.5; // Step end position\n"
        "\n"
        "function V(x, t) {\n"
        "    if((x > si) && (x < sf)) {\n"
        "        return V0;\n"
        "    }\n"
        "    else {\n"
        "        return 0;\n"
        "    }\n"
        "}";

    constexpr const char HarmonicOscillatorV[] =
        "// Harmonic oscillator potential\n"
        "omega = 2; // Frequency\n"
        "\n"
        "function V(x, t) {\n"
        "    return 0.5 * m*omega**2 * x**2;\n"
        "}";

}

constexpr const char *VDemoSources[] = {
    def_v::NoneV,
    def_v::InfiniteWellV,
    def_v::DiracDeltaV,
    def_v::StepV,
    def_v::HarmonicOscillatorV
};

constexpr const char *VDemoSourceNames[] = {
    "No potential",
    "Infinite well",
    "Dirac delta",
    "Finite step",
    "Harmonic oscillator"
};

constexpr size_t VDemoSourceCount = std::size(VDemoSources);

constexpr auto DefaultVSource = def_v::NoneV;
