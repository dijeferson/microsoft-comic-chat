// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_semantics.h"
using namespace comic;

static const double PI = 3.14159265358979323846;
static float EM(int k) { return float(k * 2 * PI / 8); }
static const float EM_HAPPY = EM(0), EM_SHOUT = EM(6), EM_LAUGH = EM(7), EM_SAD = EM(4), EM_COY = EM(1);
static const float EM_WAVE = 1001.0f, EM_POINTSELF = 1003.0f, EM_POINTOTHER = 1002.0f;

static bool has(EmotionOpts& o, float emotion, int minPriority) {
    for (auto& e : o.items()) if (e.emotion == emotion && e.priority >= minPriority) return true;
    return false;
}

int main() {
    { auto o = emotionsFromText("HELLO!!!"); assert(has(o, EM_SHOUT, 9)); }
    { auto o = emotionsFromText("that is funny :)"); assert(has(o, EM_HAPPY, 10)); }
    { auto o = emotionsFromText("oh no :("); assert(has(o, EM_SAD, 10)); }
    { auto o = emotionsFromText("lol that rocks"); assert(has(o, EM_LAUGH, 11)); }
    { auto o = emotionsFromText("Hello there"); assert(has(o, EM_WAVE, 5)); }
    { auto o = emotionsFromText("I think so"); assert(has(o, EM_POINTSELF, 3)); }
    { auto o = emotionsFromText("You are great"); assert(has(o, EM_POINTOTHER, 4)); }
    { auto o = emotionsFromText(";)"); assert(has(o, EM_COY, 10)); }
    { auto o = emotionsFromText("just some plain text"); assert(o.count() == 0); }
    std::printf("test_semantics OK\n");
    return 0;
}
