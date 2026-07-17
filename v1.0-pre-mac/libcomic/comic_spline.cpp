// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Port of CBeta / CSpline from spline.cpp. See comic_spline.h for the parity
// note. Everything here mirrors the original math exactly.

#include "comic_spline.h"

namespace comic {

BetaSpline::BetaSpline(const std::vector<Point>& controlPts, bool closed,
                       double tension, double bias)
    : cps_(controlPts), closed_(closed) {
    setMatrix(tension, bias);
    computeBezpts();
}

// CBeta::KnotCount(): closed ? nCps + 3 : nCps + 4.
int BetaSpline::knotCount() const {
    int n = static_cast<int>(cps_.size());
    return closed_ ? n + 3 : n + 4;
}

// Verbatim from CBeta::SetMatrix (spline.cpp). The map cache in the original was
// only a perf detail; we compute directly.
void BetaSpline::setMatrix(double tension, double bias) {
    double b2 = bias * bias;
    double b3 = bias * b2;
    double d = 1.0 / (tension + (2.0 * b3) + (4.0 * (b2 + bias)) + 2.0);

    matrix_[0][0] = -2.0 * b3;
    matrix_[0][1] = 2.0 * (tension + b3 + b2 + bias);
    matrix_[0][2] = -2.0 * (tension + b2 + bias + 1.0);
    matrix_[1][0] = 6.0 * b3;
    matrix_[1][1] = -3.0 * (tension + (2.0 * (b3 + b2)));
    matrix_[1][2] = 3.0 * (tension + 2.0 * b2);
    matrix_[2][0] = -6.0 * b3;
    matrix_[2][1] = 6.0 * (b3 - bias);
    matrix_[2][2] = 6.0 * bias;
    matrix_[3][0] = 2.0 * b3;
    matrix_[3][1] = tension + (4.0 * (b2 + bias));
    matrix_[0][3] = matrix_[3][2] = 2.0;
    matrix_[1][3] = matrix_[2][3] = matrix_[3][3] = 0.0;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            matrix_[i][j] *= d;
}

// CSpline::GetKnot. For closed splines the ring wraps; for open splines the
// endpoints are duplicated GetDups() times.
Point BetaSpline::getKnot(int index) const {
    int n = static_cast<int>(cps_.size());
    if (closed_) {
        if (index == 0)
            return cps_[n - 1];
        else if (index == n + 1)
            return cps_[0];
        else if (index == n + 2)
            return cps_[1];
        else
            return cps_[index - 1];
    } else {
        int dups = kDups;
        if (index < dups)
            return cps_[0];
        else if (index >= n + dups - 2)
            return cps_[n - 1];
        else
            return cps_[index - dups + 1];
    }
}

// CSpline::CvertsToCubic — matrix * [k0 k1 k2 k3], rounded per component.
void BetaSpline::cvertsToCubic(const Point& k0, const Point& k1, const Point& k2,
                               const Point& k3, Point& c0, Point& c1, Point& c2,
                               Point& c3) const {
    const auto& m = matrix_;
    c3.x = splineRound(m[0][0] * k0.x + m[0][1] * k1.x + m[0][2] * k2.x + m[0][3] * k3.x);
    c3.y = splineRound(m[0][0] * k0.y + m[0][1] * k1.y + m[0][2] * k2.y + m[0][3] * k3.y);
    c2.x = splineRound(m[1][0] * k0.x + m[1][1] * k1.x + m[1][2] * k2.x + m[1][3] * k3.x);
    c2.y = splineRound(m[1][0] * k0.y + m[1][1] * k1.y + m[1][2] * k2.y + m[1][3] * k3.y);
    c1.x = splineRound(m[2][0] * k0.x + m[2][1] * k1.x + m[2][2] * k2.x + m[2][3] * k3.x);
    c1.y = splineRound(m[2][0] * k0.y + m[2][1] * k1.y + m[2][2] * k2.y + m[2][3] * k3.y);
    c0.x = splineRound(m[3][0] * k0.x + m[3][1] * k1.x + m[3][2] * k2.x + m[3][3] * k3.x);
    c0.y = splineRound(m[3][0] * k0.y + m[3][1] * k1.y + m[3][2] * k2.y + m[3][3] * k3.y);
}

// CSpline::CubicToBezier — convert cubic coefficients to Bezier control points.
void BetaSpline::cubicToBezier(const Point& c0, const Point& c1, const Point& c2,
                               const Point& c3, Point& b0, Point& b1, Point& b2,
                               Point& b3) {
    b0.x = c0.x;
    b0.y = c0.y;
    b1.x = c0.x + splineRound((1.0 / 3.0) * c1.x);
    b1.y = c0.y + splineRound((1.0 / 3.0) * c1.y);
    b2.x = b1.x + splineRound((1.0 / 3.0) * (c1.x + c2.x));
    b2.y = b1.y + splineRound((1.0 / 3.0) * (c1.y + c2.y));
    b3.x = c0.x + c1.x + c2.x + c3.x;
    b3.y = c0.y + c1.y + c2.y + c3.y;
}

// CSpline::ComputeBezpts.
void BetaSpline::computeBezpts() {
    int nKnots = knotCount();
    if (nKnots < 4) return;

    bezpts_.assign(bezierCount(), Point{});

    int bezIndex = 1;
    Point knot0 = getKnot(0);
    Point knot1 = getKnot(1);
    Point knot2 = getKnot(2);
    Point knot3 = getKnot(3);
    Point c0, c1, c2, c3;
    Point b0, b1, b2, b3;
    for (int i = 0;; i++) {
        cvertsToCubic(knot0, knot1, knot2, knot3, c0, c1, c2, c3);
        cubicToBezier(c0, c1, c2, c3, b0, b1, b2, b3);
        if (i == 0) bezpts_[0] = b0;
        bezpts_[bezIndex] = b1;
        bezpts_[bezIndex + 1] = b2;
        bezpts_[bezIndex + 2] = b3;
        if (i + 4 == nKnots) return;
        bezIndex += 3;
        knot0 = knot1;
        knot1 = knot2;
        knot2 = knot3;
        knot3 = getKnot(i + 4);
    }
}

} // namespace comic
