// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_angles.h — angle normalization from the original vector2d.cpp. PI is
// deliberately the original's imprecise 3.14159 (pose-matching parity).

#ifndef COMIC_ANGLES_H
#define COMIC_ANGLES_H

namespace comic {

constexpr double kPI = 3.14159;

// Normalize an angle to (-PI, PI]. Ported from vector2d.cpp value_to_angle.
inline double valueToAngle(double value) {
    if (value > -kPI && value <= kPI) return value;
    double temp = value / (2 * kPI);
    temp = (temp - (int)temp) * 2 * kPI; // retain fractional component
    if (temp > kPI) return temp - 2 * kPI;
    else if (temp <= -kPI) return temp + 2 * kPI;
    else return temp;
}

// Ported from vector2d.cpp subtract_angles.
inline double subtractAngles(double a, double b) { return valueToAngle(a - b); }

} // namespace comic

#endif // COMIC_ANGLES_H
