// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// CoreGraphicsRenderer — IComicRenderer over a CGContext + Core Text. Bridges
// the portable layout core to macOS drawing. Objective-C++ so C++ and Cocoa
// coexist with no interop shim.

#ifndef CORE_GRAPHICS_RENDERER_H
#define CORE_GRAPHICS_RENDERER_H

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include "comic_renderer.h"

namespace comic {

class CoreGraphicsRenderer : public IComicRenderer {
public:
    // ctx is not owned. `heightPts` is the panel height, used to flip the
    // engine's Y-down layout coordinates into CoreGraphics' Y-up space.
    CoreGraphicsRenderer(CGContextRef ctx, int heightPts);

    Size measureText(const char* utf8, FontHandle font) override;
    void fontMetrics(FontHandle font, int& lineHeight, int& ascent) override;
    void drawImage(ImageHandle img, const Rect& dest) override;
    void beginPath() override;
    void moveTo(const Point& p) override;
    void lineTo(const Point& p) override;
    void addQuadCurveTo(const Point& control, const Point& end) override;
    void addCubicCurveTo(const Point& c1, const Point& c2, const Point& end) override;
    void addEllipseInRect(const Rect& rect) override;
    void closeSubpath() override;
    void fillAndStrokePath(const RGBA& fill, const StrokeStyle& stroke) override;
    void drawText(const char* utf8, const Point& at, FontHandle font, const RGBA& color) override;

private:
    CGContextRef ctx_;
    int height_;
    CGMutablePathRef path_ = nullptr;

    // Flip a layout point (Y-down, origin top-left) to CG (Y-up).
    CGPoint flip(const Point& p) const { return CGPointMake(p.x, height_ - p.y); }
    CTLineRef makeLine(const char* utf8, FontHandle font, const RGBA* color) const;
};

} // namespace comic

#endif // CORE_GRAPHICS_RENDERER_H
