// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_spline.h — faithful port of the beta-spline used by the CBWoodring
// balloon shapes (spline.cpp / splinutl.cpp CBeta). Given a ring of control
// points, ComputeBezpts() produces the flattened cubic-Bezier control points
// the balloon outline is stroked from.
//
// PARITY NOTE (from the port source-map, risk: spline pixel parity): the beta
// matrix formula, the ROUND() macro semantics ((int)(fp + 0.5)), the knot
// duplication in GetKnot(), and BezierCount()==3*KnotCount()-8 are reproduced
// verbatim from the original. Do NOT "clean up" these constants or the rounding
// — the shape of the Woodring balloon depends on them bit-for-bit.

#ifndef COMIC_SPLINE_H
#define COMIC_SPLINE_H

#include <vector>

#include "comic_types.h"

namespace comic {

// Original: #define ROUND(fp) ((int)(fp + 0.5))  (vector2d.h)
inline long splineRound(double fp) { return static_cast<long>(fp + 0.5); }

// Beta-spline over integer control points. Mirrors CBeta : CSpline. Default
// tension/bias match spline.cpp (defaultTension=5.0, defaultBias=1.0).
class BetaSpline {
public:
    BetaSpline(const std::vector<Point>& controlPts, bool closed,
               double tension = 5.0, double bias = 1.0);

    // Flattened cubic-Bezier control points. For a closed spline this has
    // 3*nCps + 1 entries: bezpts[0] is the start, then each group of three
    // (bezpts[1..3], bezpts[4..6], …) is one cubic segment's (c1, c2, end).
    const std::vector<Point>& bezpts() const { return bezpts_; }

    int nCps() const { return static_cast<int>(cps_.size()); }
    bool closed() const { return closed_; }

    // Original CSpline::BezierCount(): 3*KnotCount() - 8. Number of bezpts.
    int knotCount() const;
    int bezierCount() const { return 3 * knotCount() - 8; }

private:
    std::vector<Point> cps_;
    bool closed_;
    double matrix_[4][4];
    std::vector<Point> bezpts_;

    static const int kDups = 3; // CBeta::GetDups()

    void setMatrix(double tension, double bias);
    Point getKnot(int index) const;
    void computeBezpts();
    void cvertsToCubic(const Point& k0, const Point& k1, const Point& k2,
                       const Point& k3, Point& c0, Point& c1, Point& c2,
                       Point& c3) const;
    static void cubicToBezier(const Point& c0, const Point& c1, const Point& c2,
                              const Point& c3, Point& b0, Point& b1, Point& b2,
                              Point& b3);
};

} // namespace comic

#endif // COMIC_SPLINE_H
