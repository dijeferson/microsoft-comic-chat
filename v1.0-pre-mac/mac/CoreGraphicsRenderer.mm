// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#import "CoreGraphicsRenderer.h"

namespace comic {

CoreGraphicsRenderer::CoreGraphicsRenderer(CGContextRef ctx, int heightPts)
    : ctx_(ctx), height_(heightPts) {}

CTLineRef CoreGraphicsRenderer::makeLine(const char* utf8, FontHandle font,
                                         const RGBA* color) const {
    CTFontRef f = static_cast<CTFontRef>(const_cast<void*>(font));
    CFStringRef s = CFStringCreateWithCString(nullptr, utf8, kCFStringEncodingUTF8);
    if (!s) s = CFStringCreateWithCString(nullptr, "", kCFStringEncodingUTF8);

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
        nullptr, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kCTFontAttributeName, f);
    CGColorRef cg = nullptr;
    if (color) {
        cg = CGColorCreateGenericRGB(color->r / 255.0, color->g / 255.0,
                                     color->b / 255.0, color->a / 255.0);
        CFDictionarySetValue(attrs, kCTForegroundColorAttributeName, cg);
    }
    CFAttributedStringRef as = CFAttributedStringCreate(nullptr, s, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(s);
    if (cg) CGColorRelease(cg);
    return line;
}

Size CoreGraphicsRenderer::measureText(const char* utf8, FontHandle font) {
    CTLineRef line = makeLine(utf8, font, nullptr);
    CGFloat ascent = 0, descent = 0, leading = 0;
    double w = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    CFRelease(line);
    return Size{static_cast<int>(w + 0.5),
                static_cast<int>(ascent + descent + leading + 0.5)};
}

void CoreGraphicsRenderer::fontMetrics(FontHandle font, int& lineHeight, int& ascent) {
    CTFontRef f = static_cast<CTFontRef>(const_cast<void*>(font));
    CGFloat a = CTFontGetAscent(f), d = CTFontGetDescent(f), l = CTFontGetLeading(f);
    ascent = static_cast<int>(a + 0.5);
    lineHeight = static_cast<int>(a + d + l + 0.5);
}

void CoreGraphicsRenderer::drawImage(ImageHandle img, const Rect& dest) {
    CGImageRef image = static_cast<CGImageRef>(const_cast<void*>(img));
    if (!image) return;
    // dest is Y-down top-left; convert to a CG rect (Y-up bottom-left origin).
    CGRect r = CGRectMake(dest.left, height_ - dest.bottom, dest.width(), dest.height());
    CGContextSetInterpolationQuality(ctx_, kCGInterpolationHigh);
    CGContextDrawImage(ctx_, r, image);
}

void CoreGraphicsRenderer::beginPath() {
    if (path_) CGPathRelease(path_);
    path_ = CGPathCreateMutable();
}
void CoreGraphicsRenderer::moveTo(const Point& p) {
    CGPoint c = flip(p);
    CGPathMoveToPoint(path_, nullptr, c.x, c.y);
}
void CoreGraphicsRenderer::lineTo(const Point& p) {
    CGPoint c = flip(p);
    CGPathAddLineToPoint(path_, nullptr, c.x, c.y);
}
void CoreGraphicsRenderer::addQuadCurveTo(const Point& control, const Point& end) {
    CGPoint cc = flip(control), ce = flip(end);
    CGPathAddQuadCurveToPoint(path_, nullptr, cc.x, cc.y, ce.x, ce.y);
}
void CoreGraphicsRenderer::addCubicCurveTo(const Point& c1, const Point& c2,
                                           const Point& end) {
    CGPoint p1 = flip(c1), p2 = flip(c2), pe = flip(end);
    CGPathAddCurveToPoint(path_, nullptr, p1.x, p1.y, p2.x, p2.y, pe.x, pe.y);
}
void CoreGraphicsRenderer::addEllipseInRect(const Rect& rect) {
    // rect is Y-down top-left layout space; flip to a Y-up CG rect.
    CGRect r = CGRectMake(rect.left, height_ - rect.bottom, rect.width(), rect.height());
    CGPathAddEllipseInRect(path_, nullptr, r);
}
void CoreGraphicsRenderer::closeSubpath() {
    CGPathCloseSubpath(path_);
}

void CoreGraphicsRenderer::fillAndStrokePath(const RGBA& fill, const StrokeStyle& stroke) {
    if (!path_) return;
    CGContextAddPath(ctx_, path_);
    CGContextSetRGBFillColor(ctx_, fill.r / 255.0, fill.g / 255.0, fill.b / 255.0, fill.a / 255.0);
    CGContextSetRGBStrokeColor(ctx_, stroke.color.r / 255.0, stroke.color.g / 255.0,
                               stroke.color.b / 255.0, stroke.color.a / 255.0);
    CGContextSetLineWidth(ctx_, stroke.width);
    CGContextSetLineJoin(ctx_, kCGLineJoinRound);
    // Dash pattern (empty = solid, preserving all existing callers).
    if (!stroke.dashLengths.empty()) {
        std::vector<CGFloat> lengths(stroke.dashLengths.begin(),
                                     stroke.dashLengths.end());
        CGContextSetLineDash(ctx_, 0.0, lengths.data(), lengths.size());
    } else {
        CGContextSetLineDash(ctx_, 0.0, nullptr, 0);
    }
    CGContextDrawPath(ctx_, kCGPathFillStroke);
    // Reset so the dash doesn't leak into subsequent paths sharing this context.
    CGContextSetLineDash(ctx_, 0.0, nullptr, 0);
    CGPathRelease(path_);
    path_ = nullptr;
}

void CoreGraphicsRenderer::drawText(const char* utf8, const Point& at, FontHandle font,
                                    const RGBA& color) {
    // `at` is the top-left of the text in layout space. CoreText draws from the
    // baseline, so drop by the ascent and flip.
    int lineHeight = 0, ascent = 0;
    fontMetrics(font, lineHeight, ascent);
    CTLineRef line = makeLine(utf8, font, &color);
    CGContextSetTextPosition(ctx_, 0, 0);
    CGFloat baselineY = height_ - (at.y + ascent);
    CGContextSaveGState(ctx_);
    CGContextSetTextMatrix(ctx_, CGAffineTransformIdentity);
    CGContextTranslateCTM(ctx_, at.x, baselineY);
    CTLineDraw(line, ctx_);
    CGContextRestoreGState(ctx_);
    CFRelease(line);
}

} // namespace comic
