// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
using namespace comic;
int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: %s <dir> <simple> <complex>\n", argv[0]); return 2; }
    const std::string dir = argv[1];
    auto s = Avatar::load(dir, argv[2]);
    assert(s && !s->isComplex());
    ComposedBody shout = s->composeBodyForText("HELLO!!!");
    assert(shout.valid());
    ComposedBody plain = s->composeBodyForText("just text here");
    assert(plain.valid());
    auto c = Avatar::load(dir, argv[3]);
    assert(c && c->isComplex());
    ComposedBody happy = c->composeBodyForText("nice one :)");
    assert(happy.valid());
    std::printf("emotion_pose OK: simple shout %dx%d, complex happy %dx%d\n",
                shout.width, shout.height, happy.width, happy.height);
    return 0;
}
