// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_semantics.h"
#include "comic_emotions.h"
using namespace comic;

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
