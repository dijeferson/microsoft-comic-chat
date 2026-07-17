// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
using namespace comic;
int main(int argc, char** argv) {
    // argv: <avatarDir> <complexName>
    if (argc != 3) { std::fprintf(stderr, "usage: %s <avatarDir> <name>\n", argv[0]); return 2; }
    auto av = Avatar::load(argv[1], argv[2]);
    assert(av && "complex avatar should load");
    assert(av->isComplex() && "should be AT_COMPLEX");
    assert(av->faceCount() > 0 && "has faces");
    assert(av->torsoCount() > 0 && "has torsos");
    std::printf("%s: complex=%d faces=%d torsos=%d flags=%u\n",
                argv[2], av->isComplex(), av->faceCount(), av->torsoCount(), av->flags());
    return 0;
}
