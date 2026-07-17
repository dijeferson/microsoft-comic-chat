// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// render_page — headless render of a MULTI-panel comic page: one panel per
// input line, composed via composeBodyForText and tiled via PageLayout into one
// PNG. Proves the per-cell coordinate transform used by the scrollable app view.
//
// Usage: render_page <avatarDir> <name> <cols> <out.png> "line1" "line2" ...

#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "CoreGraphicsRenderer.h"
#include "comic_avatar.h"
#include "comic_compose.h"
#include "comic_page.h"
#include "comic_panel.h"

static CGImageRef MakeImage(const std::vector<comic::u8>& rgba, int w, int h) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmp = CGBitmapContextCreate((void*)rgba.data(), w, h, 8, w * 4, cs,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef img = CGBitmapContextCreateImage(bmp);
    CGContextRelease(bmp);
    CGColorSpaceRelease(cs);
    return img;
}

static bool SavePng(CGImageRef img, NSString* path) {
    CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:path];
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
    if (!dst) return false;
    CGImageDestinationAddImage(dst, img, nullptr);
    bool ok = CGImageDestinationFinalize(dst);
    CFRelease(dst);
    return ok;
}

int main(int argc, const char* argv[]) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s <avatarDir> <name> <cols> <out.png> \"line\" ...\n", argv[0]);
        return 2;
    }
    @autoreleasepool {
        std::string dir = argv[1], name = argv[2];
        int cols = atoi(argv[3]);
        if (cols < 1) cols = 1;
        NSString* out = [NSString stringWithUTF8String:argv[4]];
        std::vector<std::string> lines;
        for (int i = 5; i < argc; ++i) lines.push_back(argv[i]);

        auto av = comic::Avatar::load(dir, name);
        if (!av) { fprintf(stderr, "FAIL load %s\n", name.c_str()); return 1; }

        CTFontRef font = CTFontCreateWithName(CFSTR("Comic Sans MS"), 18.0, nullptr);
        if (!font) font = CTFontCreateWithName(CFSTR("Helvetica"), 18.0, nullptr);

        const int panelW = 300, panelH = 400, gap = 12;
        comic::PageLayout layout(panelW, panelH, cols, gap, gap);
        int n = (int)lines.size();
        int W = layout.pageWidth();
        int H = layout.pageHeight(n);
        if (H <= 0) { fprintf(stderr, "FAIL no lines\n"); return 1; }

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(nullptr, W, H, 8, W * 4, cs,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);

        // Gray page background.
        CGContextSetRGBFillColor(ctx, 0.85, 0.85, 0.85, 1.0);
        CGContextFillRect(ctx, CGRectMake(0, 0, W, H));

        for (int i = 0; i < n; ++i) {
            comic::ComposedBody cb = av->composeBodyForText(lines[i], /*maskInsideIsHigh=*/true);
            CGImageRef body = cb.valid() ? MakeImage(cb.rgba, cb.width, cb.height) : nullptr;

            // Top-down cell rect -> bottom-left origin in the y-up bitmap.
            comic::Rect cell = layout.cellRect(i);
            CGFloat cellBottomUpY = H - cell.bottom; // flip the cell's top-down y
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx, cell.left, cellBottomUpY);

            comic::CoreGraphicsRenderer renderer(ctx, panelH);
            comic::Panel panel(panelW, panelH, (const void*)font);
            if (body) {
                comic::PanelBody pb; pb.image = (const void*)body; pb.width = cb.width; pb.height = cb.height;
                panel.setBody(pb);
            }
            panel.setText(lines[i]);
            panel.draw(renderer);

            CGContextRestoreGState(ctx);
            if (body) CGImageRelease(body);
        }

        CGImageRef result = CGBitmapContextCreateImage(ctx);
        bool ok = SavePng(result, out);
        CGImageRelease(result);
        CGContextRelease(ctx);
        fprintf(ok ? stdout : stderr, "%s %s (%d panels, %dx%d)\n",
                ok ? "wrote" : "FAIL write", argv[4], n, W, H);
        return ok ? 0 : 1;
    }
}
