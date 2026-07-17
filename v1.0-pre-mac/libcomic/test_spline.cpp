// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Guards the CBeta spline port + the balloon ring builders: determinism (no
// randomness slipped in) and the expected structural point counts for a known
// input. If the spline math or AddWavies constants drift, these fire.

#include <cassert>
#include <cstdio>
#include <vector>

#include "comic_balloon.h"
#include "comic_spline.h"

using namespace comic;

int main() {
    // --- BetaSpline structural invariant: closed spline over n control points
    // yields 3n+1 flattened Bezier points (BezierCount = 3*KnotCount()-8, and
    // KnotCount = nCps+3 for a closed spline). ---------------------------------
    std::vector<Point> square{{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    BetaSpline sp(square, /*closed=*/true);
    assert(sp.knotCount() == 4 + 3);
    assert(sp.bezierCount() == 3 * 7 - 8);        // 13
    assert(static_cast<int>(sp.bezpts().size()) == sp.bezierCount());
    assert(static_cast<int>(sp.bezpts().size()) == 3 * 4 + 1);

    // Determinism: same input -> identical bezpts (bit-for-bit).
    BetaSpline sp2(square, true);
    assert(sp.bezpts().size() == sp2.bezpts().size());
    for (size_t i = 0; i < sp.bezpts().size(); i++) {
        assert(sp.bezpts()[i].x == sp2.bezpts()[i].x);
        assert(sp.bezpts()[i].y == sp2.bezpts()[i].y);
    }

    // --- Known-input wavy ring: for box {100,100}-{300,200} (expanded by the
    // 9pt border to a 218x118 rectangle) AddWavies adds 4+1+4+1 = 10 bump points
    // to the 4 corners -> 14 control points -> 3*14+1 = 43 bezpts. ------------
    Rect box{100, 100, 300, 200};
    std::vector<Point> ring = buildWavyRing(box, /*scallop=*/false);
    assert(ring.size() == 14u);
    BetaSpline wavy(ring, true);
    assert(static_cast<int>(wavy.bezpts().size()) == 3 * 14 + 1); // 43

    // Ring builders are deterministic across calls.
    std::vector<Point> ring2 = buildWavyRing(box, false);
    assert(ring == ring2);

    // Shout ring: `spikes` spikes -> 2*spikes control points.
    std::vector<Point> shout = buildShoutRing(box, 12);
    assert(shout.size() == 24u);
    std::vector<Point> shout2 = buildShoutRing(box, 12);
    assert(shout == shout2);

    std::printf("test_spline OK (bezpts=%zu ring=%zu shout=%zu)\n",
                wavy.bezpts().size(), ring.size(), shout.size());
    return 0;
}
