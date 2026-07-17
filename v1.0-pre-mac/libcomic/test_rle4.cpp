// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// test_rle4 — verifies a BI_RLE4 pose decodes to a valid bitmap. Uses a real
// RLE4 character (bolo) from the bundled art.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
#include "comic_compose.h"
using namespace comic;
int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: %s <avatarDir> <rle4Name>\n", argv[0]); return 2; }
    auto av = Avatar::load(argv[1], argv[2]);
    assert(av && "avatar should load");
    ComposedBody body = av->composeNeutralBody(true);
    assert(body.valid() && "RLE4 character must compose to a non-empty body");
    assert((int)body.rgba.size() == body.width * body.height * 4);
    std::printf("%s: composed %dx%d (RLE4 ok)\n", argv[2], body.width, body.height);
    return 0;
}
