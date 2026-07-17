// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_avatar.h — minimal .avb loader. Ports both the AT_SIMPLE path (body
// records) and the AT_COMPLEX path (separate face + torso records) of
// avatario.cpp: reads the keyed header + records, capturing each pose's byte
// offsets so a Dib can be decoded on demand. Composition of complex faces +
// torsos into pixels is a later task; see the port design doc.

#ifndef COMIC_AVATAR_H
#define COMIC_AVATAR_H

#include <optional>
#include <string>
#include <vector>

#include "comic_compose.h"
#include "comic_dib.h"
#include "comic_semantics.h"
#include "comic_types.h"

namespace comic {

// One drawable pose: byte offsets into the .avb of its drawing/mask/aura BMPs.
struct PoseRef {
    u32 fgndOffset = 0; // the visible drawing (always present)
    u32 transOffset = 0; // 1-bit mask (0 = none)
    u32 auraOffset = 0;  // nimbus/aura (0 = none)
};

// One body record (AT_SIMPLE / RBODYREC): which pose + its emotion coordinates.
struct BodyRec {
    int poseIndex = 0;   // index into Avatar::poses
    float emotion = 0.0f;
    float intensity = 0.0f;
    u8 faceX = 0;
    u8 faceY = 0;
};

// AT_COMPLEX head record (ported from FACEREC).
struct FaceRec {
    int poseIndex = 0;
    float emotion = 0.0f;
    float intensity = 0.0f;
    i16 xCX = 0, yCX = 0, deltaXCX = 0, deltaYCX = 0;
    u8 faceX = 0, faceY = 0;
};

// AT_COMPLEX torso record (ported from TORSOREC).
struct TorsoRec {
    int poseIndex = 0;
    float emotion = 0.0f;
    float intensity = 0.0f;
    i16 xCX = 0, yCX = 0;
};

class Avatar {
public:
    // Load `<dir>/<name>.avb`. Returns nullopt if the file is missing/malformed
    // or is not an AT_SIMPLE/AT_COMPLEX avatar.
    static std::optional<Avatar> load(const std::string& dir, const std::string& name);

    const std::string& name() const { return name_; }
    const std::string& path() const { return path_; }
    int bodyCount() const { return static_cast<int>(bodies_.size()); }
    const BodyRec& body(int i) const { return bodies_[i]; }

    bool isComplex() const { return complex_; }
    int faceCount() const { return static_cast<int>(faces_.size()); }
    int torsoCount() const { return static_cast<int>(torsos_.size()); }
    u8 flags() const { return flags_; }

    // Pick the body nearest the neutral pose (emotion 0, intensity 0). For the
    // MVP we always render neutral; text->emotion selection comes later.
    int neutralBodyIndex() const;

    // Decode the drawing DIB for a given body's pose. Returns invalid Dib on
    // failure. Opens the .avb fresh each call (poses are cached by the caller).
    Dib loadDrawing(int bodyIndex) const;

    // Neutral selection for complex avatars.
    int neutralFaceIndex() const;
    int neutralTorsoIndex() const;

    // Compose the neutral body into a single RGBA bitmap. Works for both
    // AT_SIMPLE (one part) and AT_COMPLEX (head+torso). Empty ComposedBody on
    // failure. When `drawAura` is true, each pose's aura/nimbus DIB (if present)
    // is composited UNDER the body as a white glow (see compositeAura); the
    // default (false) preserves the original no-aura composite exactly.
    ComposedBody composeNeutralBody(bool maskInsideIsHigh = true,
                                    bool drawAura = false) const;

    // Emotion→pose selection (ported from avatar.cpp). For AT_SIMPLE, choose the
    // body record nearest the requested emotion/intensity; for AT_COMPLEX, choose
    // face + torso records.
    int bodyIndexForEmotion(float emotion, float intensity) const;
    void faceTorsoForEmotion(float emotion, float intensity, int& faceIndex, int& torsoIndex) const;

    // Run the text→emotion rule engine, pick the highest-priority pose that
    // matches, composite it, and fall back to neutral on no match. `drawAura`
    // behaves as in composeNeutralBody (default off = original behavior).
    ComposedBody composeBodyForText(const std::string& text, bool maskInsideIsHigh = true,
                                    bool drawAura = false) const;

private:
    Dib loadDibAt(u32 offset) const;
    Dib loadPoseDrawing(int poseIndex) const;
    Dib loadPoseMask(int poseIndex) const;
    Dib loadPoseAura(int poseIndex) const;

    ComposedBody composeFromIndices(int bodyIndex, int faceIndex, int torsoIndex,
                                    bool maskInsideIsHigh, bool drawAura) const;

    std::string name_;
    std::string path_;
    std::vector<PoseRef> poses_;
    std::vector<BodyRec> bodies_;
    bool complex_ = false;
    u8 flags_ = 0;
    std::vector<FaceRec> faces_;
    std::vector<TorsoRec> torsos_;
};

} // namespace comic

#endif // COMIC_AVATAR_H
