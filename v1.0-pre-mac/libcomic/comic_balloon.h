// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_balloon.h — ornate balloon outlines selected by speech mode. Ports the
// intent of balloon.cpp's CBWoodring family:
//   Say     -> CBWoodringNormal : wavy beta-spline outline + tail
//   Think   -> CBWoodringThink  : scalloped cloud + trail of bubbles (no tail)
//   Whisper -> CBWoodringWhisper: same shape, thin light outline (the nimbus)
//   Shout   -> (was stubbed out) : spiky/jagged burst outline + tail
//
// The wavy outline is built by AddWavies (ported verbatim in intent from
// balloon.cpp) feeding a closed CBeta spline (comic_spline.h). Given the port's
// point-space (vs the engine's TWIP space) the wave interval/height constants
// are scaled down; the spline math itself is bit-faithful.

#ifndef COMIC_BALLOON_H
#define COMIC_BALLOON_H

#include <vector>

#include "comic_renderer.h"
#include "comic_types.h"

namespace comic {

enum class SpeechMode { Say, Think, Whisper, Shout };

// --- Testable geometry (no renderer) --------------------------------------

// Build the ring of control points for a wavy balloon around `box` (Say / Think
// / Whisper). Deterministic: the wave points are placed by AddWavies with fixed
// intervals — no randomness. `scallop` uses the larger think-cloud wave height.
std::vector<Point> buildWavyRing(const Rect& box, bool scallop);

// Build the spiky burst ring for Shout: `spikes` outer points alternating with
// inner points around the ellipse inscribed in `box`.
std::vector<Point> buildShoutRing(const Rect& box, int spikes);

// --- Rendering ------------------------------------------------------------

// Draw the balloon OUTLINE (body + tail/bubbles) for `mode` around the text box
// `box`, with the tail pointing at `tailTarget`. Fills white + strokes black
// (whisper strokes thin+light). Does NOT draw the text — the panel does that.
void drawBalloon(IComicRenderer& r, SpeechMode mode, const Rect& box,
                 const Point& tailTarget);

} // namespace comic

#endif // COMIC_BALLOON_H
