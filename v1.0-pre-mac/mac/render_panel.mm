// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// render_panel — headless render of one full panel (avatar + balloon) through
// the SAME CoreGraphicsRenderer + Panel path the live app uses, into an
// offscreen CGBitmapContext saved as PNG. Verifies the render seam without
// needing screen capture. (Source-map risks #1 coordinates + balloon paths.)
//
// Usage: render_panel <avatarDir> <name> "<text>" <out.png> [mode]
//   mode = say | think | whisper | shout   (default say)

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
#include "comic_dib.h"
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

static comic::SpeechMode ParseMode(const char* s) {
    std::string m = s ? s : "say";
    if (m == "think") return comic::SpeechMode::Think;
    if (m == "whisper") return comic::SpeechMode::Whisper;
    if (m == "shout") return comic::SpeechMode::Shout;
    return comic::SpeechMode::Say;
}

int main(int argc, const char* argv[]) {
    if (argc != 5 && argc != 6) {
        fprintf(stderr, "usage: %s <avatarDir> <name> <text> <out.png> [say|think|whisper|shout]\n", argv[0]);
        return 2;
    }
    @autoreleasepool {
        std::string dir = argv[1], name = argv[2], text = argv[3];
        NSString* out = [NSString stringWithUTF8String:argv[4]];
        comic::SpeechMode mode = ParseMode(argc == 6 ? argv[5] : "say");

        auto av = comic::Avatar::load(dir, name);
        if (!av) { fprintf(stderr, "FAIL load %s\n", name.c_str()); return 1; }
        comic::ComposedBody cb = av->composeBodyForText(text, /*maskInsideIsHigh=*/true);
        if (!cb.valid()) { fprintf(stderr, "FAIL compose\n"); return 1; }
        CGImageRef body = MakeImage(cb.rgba, cb.width, cb.height);

        CTFontRef font = CTFontCreateWithName(CFSTR("Comic Sans MS"), 18.0, nullptr);
        if (!font) font = CTFontCreateWithName(CFSTR("Helvetica"), 18.0, nullptr);

        const int W = 460, H = 620;
        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(nullptr, W, H, 8, W * 4, cs,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);

        // Gray backdrop (matches the app), then the panel.
        CGContextSetRGBFillColor(ctx, 0.85, 0.85, 0.85, 1.0);
        CGContextFillRect(ctx, CGRectMake(0, 0, W, H));

        comic::CoreGraphicsRenderer renderer(ctx, H);
        comic::Panel panel(W, H, (const void*)font);
        comic::PanelBody pb; pb.image = (const void*)body; pb.width = cb.width; pb.height = cb.height;
        panel.setBody(pb);
        panel.setText(text);
        panel.setSpeechMode(mode);
        panel.draw(renderer);

        CGImageRef result = CGBitmapContextCreateImage(ctx);
        bool ok = SavePng(result, out);
        CGImageRelease(result);
        CGContextRelease(ctx);
        CGImageRelease(body);
        fprintf(ok ? stdout : stderr, "%s %s\n", ok ? "wrote" : "FAIL write", argv[4]);
        return ok ? 0 : 1;
    }
}
