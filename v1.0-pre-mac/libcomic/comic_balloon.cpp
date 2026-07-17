// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Port of the CBWoodring balloon shapes (balloon.cpp) onto the portable render
// seam. See comic_balloon.h. The wavy outline reproduces AddWavies +
// CreateBalloonSpline's closed CBeta spline; the think cloud reproduces the
// bubble trail from CBWoodringThink::Draw; whisper reuses the shape with a thin
// light stroke (the nimbus); shout is the never-shipped spiky variant.

#include "comic_balloon.h"

#include <algorithm>
#include <cmath>

#include "comic_angles.h"
#include "comic_spline.h"

namespace comic {

namespace {
constexpr RGBA kBlack{0, 0, 0, 255};
constexpr RGBA kWhite{255, 255, 255, 255};
constexpr RGBA kWhisperGray{120, 120, 120, 255};

// Outward margin from the text box to the balloon outline (was XBORDER/YBORDER
// in the engine's TWIP space; scaled to points here).
constexpr int kBorder = 9;
// Tail geometry.
constexpr int kTailHalfBase = 11;
constexpr int kMaxTailLen = 90;

// AddWavies — ported verbatim in intent from balloon.cpp. Places bump points
// between pt1 and pt2 along the edge; even waves bulge outward by waveDiam along
// the edge normal, odd waves sit on the base line. Deterministic (no rand).
void addWavies(const Point& pt1, const Point& pt2, std::vector<Point>& pts,
               int waveDiam, int interval) {
    double dx = static_cast<double>(pt2.x - pt1.x);
    double dy = static_cast<double>(pt2.y - pt1.y);
    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= 0.0) return;
    double nWaves = dist / interval;
    if (nWaves < 2) return;
    int iWaves = static_cast<int>(nWaves);
    double waveLen = dist / iWaves;
    double ux = dx / dist, uy = dy / dist;             // unit vector
    Point incVec{splineRound(waveLen * ux), splineRound(waveLen * uy)};
    double nx = uy, ny = -ux;                          // edge normal
    Point extraVec{splineRound(waveDiam * nx), splineRound(waveDiam * ny)};
    Point thisBase = pt1;
    for (int i = 0; i < iWaves - 1; i++) {
        thisBase = Point{thisBase.x + incVec.x, thisBase.y + incVec.y};
        if (!(i & 0x1))
            pts.push_back(Point{thisBase.x + extraVec.x, thisBase.y + extraVec.y});
        else
            pts.push_back(thisBase);
    }
}

// Draw a closed beta-spline outline from a control-point ring.
void strokeSplineRing(IComicRenderer& r, const std::vector<Point>& ring,
                      const RGBA& fill, const StrokeStyle& stroke) {
    BetaSpline spline(ring, /*closed=*/true);
    const std::vector<Point>& bez = spline.bezpts();
    if (bez.size() < 4) return;
    r.beginPath();
    r.moveTo(bez[0]);
    for (size_t i = 1; i + 2 < bez.size(); i += 3)
        r.addCubicCurveTo(bez[i], bez[i + 1], bez[i + 2]);
    r.closeSubpath();
    r.fillAndStrokePath(fill, stroke);
}

// Bottom-center-ish anchor of the balloon body, used as the tail base.
Point tailBase(const Rect& box, const Point& target) {
    long minX = box.left + kTailHalfBase + 2;
    long maxX = box.right - kTailHalfBase - 2;
    long bx = std::min(maxX, std::max(minX, target.x));
    return Point{bx, box.bottom + kBorder};
}

// A filled+stroked tail triangle from the balloon bottom to `target`.
void drawTail(IComicRenderer& r, const Rect& box, const Point& target,
              const StrokeStyle& stroke) {
    Point base = tailBase(box, target);
    long tipY = std::min<long>(base.y + kMaxTailLen, std::max<long>(base.y + 8, target.y));
    Point tip{target.x, tipY};
    r.beginPath();
    r.moveTo(Point{base.x - kTailHalfBase, base.y});
    r.lineTo(tip);
    r.lineTo(Point{base.x + kTailHalfBase, base.y});
    r.closeSubpath();
    r.fillAndStrokePath(kWhite, stroke);
}
} // namespace

std::vector<Point> buildWavyRing(const Rect& box, bool scallop) {
    Rect b{box.left - kBorder, box.top - kBorder, box.right + kBorder,
           box.bottom + kBorder};
    Point tl{b.left, b.top}, tr{b.right, b.top}, br{b.right, b.bottom},
        bl{b.left, b.bottom};
    // Think clouds get taller, tighter scallops; say balloons get gentle waves.
    int waveH = scallop ? 13 : 6;
    int interval = scallop ? 30 : 42;

    std::vector<Point> pts;
    pts.push_back(tl);
    addWavies(tl, tr, pts, waveH, interval);
    pts.push_back(tr);
    addWavies(tr, br, pts, waveH, interval);
    pts.push_back(br);
    addWavies(br, bl, pts, waveH, interval);
    pts.push_back(bl);
    addWavies(bl, tl, pts, waveH, interval);
    return pts;
}

std::vector<Point> buildShoutRing(const Rect& box, int spikes) {
    if (spikes < 3) spikes = 3;
    double cx = (box.left + box.right) / 2.0;
    double cy = (box.top + box.bottom) / 2.0;
    double rx = (box.right - box.left) / 2.0 + kBorder;
    double ry = (box.bottom - box.top) / 2.0 + kBorder;
    std::vector<Point> pts;
    int n = spikes * 2;
    for (int k = 0; k < n; k++) {
        double ang = kPI * k / spikes;    // n points around the ellipse
        double f = (k & 1) ? 0.66 : 1.18; // inner valley vs outer spike
        pts.push_back(Point{splineRound(cx + std::cos(ang) * rx * f),
                            splineRound(cy + std::sin(ang) * ry * f)});
    }
    return pts;
}

void drawBalloon(IComicRenderer& r, SpeechMode mode, const Rect& box,
                 const Point& tailTarget) {
    switch (mode) {
    case SpeechMode::Say: {
        StrokeStyle stroke{2.0, kBlack};
        drawTail(r, box, tailTarget, stroke);          // under the body
        strokeSplineRing(r, buildWavyRing(box, /*scallop=*/false), kWhite, stroke);
        break;
    }
    case SpeechMode::Whisper: {
        // The nimbus: thin, light-gray outline (the original filled a fat white
        // pen then dashed a thin line — we approximate with a light thin stroke).
        StrokeStyle stroke{1.0, kWhisperGray};
        drawTail(r, box, tailTarget, stroke);
        strokeSplineRing(r, buildWavyRing(box, /*scallop=*/false), kWhite, stroke);
        break;
    }
    case SpeechMode::Shout: {
        StrokeStyle stroke{2.5, kBlack};
        drawTail(r, box, tailTarget, stroke);
        std::vector<Point> ring = buildShoutRing(box, /*spikes=*/12);
        if (ring.size() >= 3) {
            r.beginPath();
            r.moveTo(ring[0]);
            for (size_t i = 1; i < ring.size(); i++) r.lineTo(ring[i]);
            r.closeSubpath();
            r.fillAndStrokePath(kWhite, stroke);
        }
        break;
    }
    case SpeechMode::Think: {
        StrokeStyle stroke{2.0, kBlack};
        // Scalloped cloud body (no tail).
        strokeSplineRing(r, buildWavyRing(box, /*scallop=*/true), kWhite, stroke);

        // Trail of shrinking bubbles from the cloud toward the speaker
        // (CBWoodringThink::Draw's Ellipse loop, scaled to points).
        Point entry{(box.left + box.right) / 2, box.bottom + kBorder};
        long deltaY = tailTarget.y - entry.y;
        if (deltaY <= 0) break;
        int nBubbles = std::min<int>(4, static_cast<int>(deltaY / 22));
        for (int i = 0; i < nBubbles; i++) {
            double t = (i + 1.0) / (nBubbles + 1.0);
            long cxp = static_cast<long>(entry.x + (tailTarget.x - entry.x) * t);
            long cyp = static_cast<long>(entry.y + deltaY * t);
            long rad = std::max<long>(3, 9 - i * 2); // shrink toward speaker
            Rect circ{cxp - rad, cyp - rad, cxp + rad, cyp + rad};
            r.beginPath();
            r.addEllipseInRect(circ);
            r.fillAndStrokePath(kWhite, stroke);
        }
        break;
    }
    }
}

} // namespace comic
