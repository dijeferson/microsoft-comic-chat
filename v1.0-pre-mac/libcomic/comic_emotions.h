// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_emotions.h — the emotion-wheel constants, defined once.
//
// IMPORTANT — two different PI values, on purpose (faithful to the original):
//   * kWheelPI (full precision) computes the emotion VALUES stored on poses and
//     produced by the rule engine — mirrors the original's emFloats/EM_* macros,
//     which used the full-precision PI.
//   * kMatchPI (the imprecise 3.14159) normalizes ANGLES during pose matching —
//     mirrors the original vector2d.cpp value_to_angle. It lives in
//     comic_angles.h as comic::kPI; matching code uses that, NOT kWheelPI.
// Do not collapse these into one constant.

#ifndef COMIC_EMOTIONS_H
#define COMIC_EMOTIONS_H

namespace comic {

// Full-precision PI used to compute emotion-wheel VALUES (not angle matching).
constexpr double kWheelPI = 3.14159265358979323846;

// The 8 emotion-wheel angles are k*2*PI/8 (values match avatario.cpp
// emFloats[1..8]). Exposed as comic::EM_* constants below.
inline float wheelEmotion(int k) { return static_cast<float>(k * 2 * kWheelPI / 8); }

// Emotion-wheel angles (indices match the original emFloats table order).
inline const float EM_HAPPY = wheelEmotion(0);
inline const float EM_COY   = wheelEmotion(1);
inline const float EM_BORED = wheelEmotion(2);
inline const float EM_SCARED= wheelEmotion(3);
inline const float EM_SAD   = wheelEmotion(4);
inline const float EM_ANGRY = wheelEmotion(5);
inline const float EM_SHOUT = wheelEmotion(6);
inline const float EM_LAUGH = wheelEmotion(7);
constexpr float EM_NEUTRAL = 0.0f;

// Gesture "emotions" are sentinels > 2*PI, so angle matching is skipped for
// them (exact-match only). Values match avatario.cpp emFloats[10..17].
constexpr float EM_WAVE        = 1001.0f;
constexpr float EM_POINTOTHER  = 1002.0f;
constexpr float EM_POINTSELF   = 1003.0f;
constexpr float EM_DOUBLEPOINT = 1004.0f;
constexpr float EM_SHRUG       = 1005.0f;
constexpr float EM_3QRWALK     = 1006.0f;
constexpr float EM_SIDEWALK    = 1007.0f;
constexpr float EM_3QFWALK     = 1008.0f;

} // namespace comic

#endif // COMIC_EMOTIONS_H
