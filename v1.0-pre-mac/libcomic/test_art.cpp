// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// test_art — headless verification of the .avb + DIB pipeline. Loads an avatar,
// decodes its neutral pose, and writes a PNG. Proves art decode works before any
// Cocoa code exists (source-map risk #3, minus mask compositing).
//
// Usage: test_art <avatarDir> <name> <out.png>

#include <cstdio>

#include "comic_avatar.h"
#include "comic_dib.h"
#include "png_writer.h"

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: %s <avatarDir> <name> <out.png>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    const std::string name = argv[2];
    const std::string out = argv[3];

    auto av = comic::Avatar::load(dir, name);
    if (!av) {
        std::fprintf(stderr, "FAIL: could not load %s/%s.avb (missing or not AT_SIMPLE)\n",
                     dir.c_str(), name.c_str());
        return 1;
    }
    std::printf("loaded %s: %d bodies\n", name.c_str(), av->bodyCount());

    int idx = av->neutralBodyIndex();
    comic::Dib dib = av->loadDrawing(idx);
    if (!dib.valid()) {
        std::fprintf(stderr, "FAIL: could not decode drawing for body %d\n", idx);
        return 1;
    }
    std::printf("neutral body %d: pose drawing %dx%d\n", idx, dib.width(), dib.height());

    // Opaque expansion (SRCAND over white == source on white panels).
    std::vector<comic::u8> rgba = dib.toRGBA(-1);
    if (!comic::writePng(out, rgba, dib.width(), dib.height())) {
        std::fprintf(stderr, "FAIL: could not write %s\n", out.c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", out.c_str(), dib.width(), dib.height());
    return 0;
}
