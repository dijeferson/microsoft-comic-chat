// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// render_panel — headless render of one full panel (avatar + balloon) through
// the SAME CoreGraphicsRenderer + Panel path the live app uses, into an
// offscreen CGBitmapContext saved as PNG. Verifies the render seam without
// needing screen capture. (Source-map risks #1 coordinates + balloon paths.)
//
// Usage: render_panel <avatarDir> <name> "<text>" <out.png> [mode] [backdrop.bmp] [aura] [emotion]
//   mode          = say | think | whisper | shout            (default say)
//   backdrop.bmp  = full path to a backdrop .bmp, or "-"/"" for none (default none)
//   aura          = 1 | on | aura | true to composite the aura glow (default off)
//   emotion       = auto | happy | sad | angry | shout | laugh | coy | bored |
//                   scared | wave | pointself | pointother                (default auto)
//                   "auto"/"-"/"" derives the pose from the text (composeBodyForText);
//                   any other value FORCES that emotion (composeBodyForEmotion, intensity 1).
//   All four are positional and optional; pass "-" for a slot you want to skip.

#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <Foundation/Foundation.h>

#include <memory>
#include <string>
#include <vector>

#include "CoreGraphicsRenderer.h"
#include "comic_avatar.h"
#include "comic_backdrop.h"
#include "comic_emotions.h"
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

// Sentinel meaning "derive the pose from the text" (composeBodyForText).
static const float kEmotionAuto = -1.0f;

// Map an emotion-name arg to a forced emotion value, or kEmotionAuto for
// auto/-/empty. Returns kEmotionAuto for unrecognized names too (safe default).
static float ParseEmotion(const char* s) {
    std::string e = s ? s : "";
    if (e.empty() || e == "-" || e == "auto") return kEmotionAuto;
    if (e == "happy")      return comic::EM_HAPPY;
    if (e == "sad")        return comic::EM_SAD;
    if (e == "angry")      return comic::EM_ANGRY;
    if (e == "shout")      return comic::EM_SHOUT;
    if (e == "laugh")      return comic::EM_LAUGH;
    if (e == "coy")        return comic::EM_COY;
    if (e == "bored")      return comic::EM_BORED;
    if (e == "scared")     return comic::EM_SCARED;
    if (e == "wave")       return comic::EM_WAVE;
    if (e == "pointself")  return comic::EM_POINTSELF;
    if (e == "pointother") return comic::EM_POINTOTHER;
    fprintf(stderr, "warning: unknown emotion '%s', using auto\n", e.c_str());
    return kEmotionAuto;
}

int main(int argc, const char* argv[]) {
    if (argc < 5 || argc > 9) {
        fprintf(stderr, "usage: %s <avatarDir> <name> <text> <out.png> [mode] [backdrop.bmp] [aura] [emotion]\n", argv[0]);
        return 2;
    }
    @autoreleasepool {
        std::string dir = argv[1], name = argv[2], text = argv[3];
        NSString* out = [NSString stringWithUTF8String:argv[4]];
        comic::SpeechMode mode = ParseMode(argc >= 6 ? argv[5] : "say");
        std::string backdropPath;
        if (argc >= 7) {
            std::string b = argv[6];
            if (b != "-" && !b.empty()) backdropPath = b;
        }
        bool drawAura = false;
        if (argc >= 8) {
            std::string a = argv[7];
            drawAura = (a == "1" || a == "on" || a == "aura" || a == "true");
        }
        float emotion = ParseEmotion(argc >= 9 ? argv[8] : "auto");

        auto av = comic::Avatar::load(dir, name);
        if (!av) { fprintf(stderr, "FAIL load %s\n", name.c_str()); return 1; }
        comic::ComposedBody cb = (emotion == kEmotionAuto)
            ? av->composeBodyForText(text, /*maskInsideIsHigh=*/true, drawAura)
            : av->composeBodyForEmotion(emotion, /*intensity=*/1.0f, /*maskInsideIsHigh=*/true, drawAura);
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

        // Optional backdrop, loaded from a full .bmp path split into dir + stem.
        CGImageRef backdropImg = nullptr;
        std::unique_ptr<comic::Backdrop> bd;
        if (!backdropPath.empty()) {
            std::string bdir = ".", bstem = backdropPath;
            size_t slash = backdropPath.find_last_of('/');
            if (slash != std::string::npos) {
                bdir = backdropPath.substr(0, slash);
                bstem = backdropPath.substr(slash + 1);
            }
            size_t dot = bstem.find_last_of('.');
            if (dot != std::string::npos) bstem = bstem.substr(0, dot);
            bd.reset(comic::Backdrop::load(bdir, bstem));
            if (!bd) { fprintf(stderr, "FAIL load backdrop %s\n", backdropPath.c_str()); return 1; }
            fprintf(stdout, "backdrop %s: %dx%d\n", bstem.c_str(), bd->width(), bd->height());
            backdropImg = MakeImage(bd->rgba(), bd->width(), bd->height());
            panel.setBackdrop((const void*)backdropImg, bd->width(), bd->height());
        }

        comic::PanelBody pb; pb.image = (const void*)body; pb.width = cb.width; pb.height = cb.height;
        panel.setBody(pb);
        panel.setText(text);
        panel.setSpeechMode(mode);
        panel.draw(renderer);
        if (backdropImg) CGImageRelease(backdropImg);

        CGImageRef result = CGBitmapContextCreateImage(ctx);
        bool ok = SavePng(result, out);
        CGImageRelease(result);
        CGContextRelease(ctx);
        CGImageRelease(body);
        fprintf(ok ? stdout : stderr, "%s %s\n", ok ? "wrote" : "FAIL write", argv[4]);
        return ok ? 0 : 1;
    }
}
