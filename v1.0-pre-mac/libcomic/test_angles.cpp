// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cmath>
#include <cstdio>
#include "comic_angles.h"
using namespace comic;
int main() {
    // Within range: identity.
    assert(std::fabs(valueToAngle(1.0) - 1.0) < 1e-9);
    // subtract_angles wraps: near +PI minus near -PI ~= 0 (not 2PI).
    double d = subtractAngles(kPI - 0.01, -(kPI - 0.01));
    assert(std::fabs(d) < 0.05); // wrapped to ~ -0.02, magnitude small
    // Opposite points on the wheel are ~PI apart.
    assert(std::fabs(std::fabs(subtractAngles(0.0, kPI)) - kPI) < 1e-6);
    std::printf("test_angles OK\n");
    return 0;
}
