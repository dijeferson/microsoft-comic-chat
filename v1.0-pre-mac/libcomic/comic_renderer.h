// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_renderer.h — the abstract seam between the portable layout core and the
// platform drawing backend. Derived from the real GDI call sites in panel.cpp /
// balloon.cpp / dib.cpp (see the source-map doc). The MVP CoreGraphics backend
// implements this; layout code and future backends depend only on this header.
//
// Coordinates here are in the RENDERER's device space (points), already mapped
// by the caller from the engine's TWIP space. The core never calls an OS API.

#ifndef COMIC_RENDERER_H
#define COMIC_RENDERER_H

#include <vector>

#include "comic_types.h"

namespace comic {

// Opaque handle to a backend image built from RGBA bits (e.g. a CGImage).
using ImageHandle = const void*;

// Opaque handle to a backend font.
using FontHandle = const void*;

struct StrokeStyle {
    double width = 1.0;
    RGBA color{0, 0, 0, 255};
};

class IComicRenderer {
public:
    virtual ~IComicRenderer() = default;

    // --- Text (measureText is also called during layout) ------------------
    // Width/height of a UTF-8 run in the given font, in points.
    virtual Size measureText(const char* utf8, FontHandle font) = 0;
    // Line height / ascent for the font, in points.
    virtual void fontMetrics(FontHandle font, int& lineHeight, int& ascent) = 0;

    // --- Bitmap -----------------------------------------------------------
    // Draw an RGBA image into dest (points). srcW/srcH describe the image.
    virtual void drawImage(ImageHandle img, const Rect& dest) = 0;

    // --- Vector paths (balloon outline + tail) ----------------------------
    virtual void beginPath() = 0;
    virtual void moveTo(const Point& p) = 0;
    virtual void lineTo(const Point& p) = 0;
    virtual void addQuadCurveTo(const Point& control, const Point& end) = 0;
    // Cubic Bezier from the current point through control points c1, c2 to end.
    // Added for the CBWoodring beta-spline balloon outlines (comic_balloon).
    virtual void addCubicCurveTo(const Point& c1, const Point& c2,
                                 const Point& end) = 0;
    // Append a full ellipse (its own closed subpath) inscribed in the rect.
    // Used for the thought-balloon cloud + its trail of bubbles.
    virtual void addEllipseInRect(const Rect& rect) = 0;
    virtual void closeSubpath() = 0;
    // Fill the current path, then stroke it, in one operation (balloons fill
    // white + stroke black — matches GDI StrokeAndFillPath).
    virtual void fillAndStrokePath(const RGBA& fill, const StrokeStyle& stroke) = 0;

    // --- Text draw --------------------------------------------------------
    // Draw a UTF-8 run with its top-left at `at` (points).
    virtual void drawText(const char* utf8, const Point& at, FontHandle font,
                          const RGBA& color) = 0;
};

} // namespace comic

#endif // COMIC_RENDERER_H
