// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_avatar.h — minimal .avb loader for the MVP. Ports the AT_SIMPLE path of
// avatario.cpp: reads the keyed header + body records, capturing each pose's
// byte offsets so a Dib can be decoded on demand. AT_COMPLEX (head+torso) is
// deferred; see the port design doc.

#ifndef COMIC_AVATAR_H
#define COMIC_AVATAR_H

#include <optional>
#include <string>
#include <vector>

#include "comic_dib.h"
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

class Avatar {
public:
    // Load `<dir>/<name>.avb`. Returns nullopt if the file is missing/malformed
    // or is not an AT_SIMPLE avatar (MVP scope).
    static std::optional<Avatar> load(const std::string& dir, const std::string& name);

    const std::string& name() const { return name_; }
    const std::string& path() const { return path_; }
    int bodyCount() const { return static_cast<int>(bodies_.size()); }
    const BodyRec& body(int i) const { return bodies_[i]; }

    // Pick the body nearest the neutral pose (emotion 0, intensity 0). For the
    // MVP we always render neutral; text->emotion selection comes later.
    int neutralBodyIndex() const;

    // Decode the drawing DIB for a given body's pose. Returns invalid Dib on
    // failure. Opens the .avb fresh each call (poses are cached by the caller).
    Dib loadDrawing(int bodyIndex) const;

private:
    std::string name_;
    std::string path_;
    std::vector<PoseRef> poses_;
    std::vector<BodyRec> bodies_;
};

} // namespace comic

#endif // COMIC_AVATAR_H
