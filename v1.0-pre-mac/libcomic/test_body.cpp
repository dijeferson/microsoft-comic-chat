// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
#include "png_writer.h"
using namespace comic;
int main(int argc, char** argv) {
    // argv: <avatarDir> <name> <out.png>
    if (argc != 4) { std::fprintf(stderr, "usage: %s <avatarDir> <name> <out.png>\n", argv[0]); return 2; }
    auto av = Avatar::load(argv[1], argv[2]);
    assert(av);
    ComposedBody body = av->composeNeutralBody(true);
    assert(body.valid() && "composed body must be non-empty");
    assert((int)body.rgba.size() == body.width * body.height * 4);
    bool wrote = writePng(argv[3], body.rgba, body.width, body.height);
    assert(wrote);
    std::printf("%s: composed %dx%d\n", argv[2], body.width, body.height);
    return 0;
}
