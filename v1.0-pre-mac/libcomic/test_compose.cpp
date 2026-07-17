// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// test_compose — assertions for stampPart. Validates the no-op-on-invalid-input
// contract (an empty drawing must not touch the canvas), which is the behavior
// the avatar compositor relies on when a pose fails to decode.

#include <cassert>
#include <cstdio>
#include <vector>

#include "comic_compose.h"

using namespace comic;

int main() {
    // Canvas 4x1, zero-filled; stampPart on an invalid Dib must leave it untouched.
    std::vector<u8> canvas(4 * 1 * 4, 0);

    // With an empty/invalid drawing, stampPart must be a no-op (no crash).
    Dib empty;
    stampPart(canvas, 4, 1, 0, 0, empty, nullptr, true, 0);
    for (u8 v : canvas) assert(v == 0);

    std::printf("test_compose OK\n");
    return 0;
}
