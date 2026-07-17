// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Minimal .avb loader for AT_SIMPLE (body records) and AT_COMPLEX (face +
// torso records), ported from avatario.cpp (LoadAvatar / LoadBodyRecs /
// LoadFaceRecs / LoadTorsoRecs / LoadBasics) and avatario.h key constants.

#include "comic_avatar.h"

#include <algorithm>
#include <cstdio>

namespace comic {

namespace {

// avatario.h key/type constants.
constexpr int kMagic = 0x81;
constexpr int kAT_SIMPLE = 1;
constexpr int kAK_NAME = 1;
constexpr int kAK_FLAGS = 2;
constexpr int kAK_ICON = 3;
constexpr int kAK_STARTDATA = 6;
constexpr int kAK_STYLE = 8;
constexpr int kAK_NBODIES = 9;
constexpr int kAT_COMPLEX = 2;
constexpr int kAK_NFACES = 4;
constexpr int kAK_NTORSOS = 5;

// The emFloats[] table from avatario.cpp. Indices 1..8 are the emotion wheel
// (k*2*PI/8), 9 is neutral (0.0), 10..17 are the >2*PI gesture sentinels.
const double kPI = 3.14159265358979323846;
float emotionToFloat(int index) {
    static const float table[] = {
        0.0f,
        float(0 * 2 * kPI / 8), float(1 * 2 * kPI / 8), float(2 * 2 * kPI / 8),
        float(3 * 2 * kPI / 8), float(4 * 2 * kPI / 8), float(5 * 2 * kPI / 8),
        float(6 * 2 * kPI / 8), float(7 * 2 * kPI / 8),
        0.0f, // EM_NEUTRAL
        1001.0f, 1002.0f, 1003.0f, 1004.0f, 1005.0f, 1006.0f, 1007.0f, 1008.0f,
    };
    int n = int(sizeof(table) / sizeof(table[0]));
    if (index < 0 || index >= n) return 0.0f;
    return table[index];
}

// Little-endian file readers matching read16/read32/read8 (read8 unsigned).
bool rdU16(std::FILE* fp, u16& out) {
    int lo = std::fgetc(fp), hi = std::fgetc(fp);
    if (lo == EOF || hi == EOF) return false;
    out = static_cast<u16>(lo | (hi << 8));
    return true;
}
bool rdI16(std::FILE* fp, i16& out) { u16 v; if (!rdU16(fp, v)) return false; out = static_cast<i16>(v); return true; }
bool rdU32(std::FILE* fp, u32& out) {
    int b0 = std::fgetc(fp), b1 = std::fgetc(fp), b2 = std::fgetc(fp), b3 = std::fgetc(fp);
    if (b0 == EOF || b1 == EOF || b2 == EOF || b3 == EOF) return false;
    out = u32(b0) | (u32(b1) << 8) | (u32(b2) << 16) | (u32(b3) << 24);
    return true;
}
bool rdU8(std::FILE* fp, u8& out) { int c = std::fgetc(fp); if (c == EOF) return false; out = static_cast<u8>(c); return true; }

} // namespace

std::optional<Avatar> Avatar::load(const std::string& dir, const std::string& name) {
    Avatar av;
    av.name_ = name;
    av.path_ = dir + "/" + name + ".avb";

    std::FILE* fp = std::fopen(av.path_.c_str(), "rb");
    if (!fp) return std::nullopt;

    u16 magic, avType, version;
    if (!rdU16(fp, magic) || !rdU16(fp, avType) || !rdU16(fp, version) ||
        magic != kMagic || (avType != kAT_SIMPLE && avType != kAT_COMPLEX)) {
        std::fclose(fp);
        return std::nullopt;
    }
    av.complex_ = (avType == kAT_COMPLEX);

    // Dedupe poses by foreground offset, mirroring the "ditto" logic.
    u32 lastOffset = 0;
    int lastPoseIndex = -1;

    auto registerPose = [&](u32 fgnd, u32 trans, u32 aura) -> int {
        if (fgnd != lastOffset || lastPoseIndex < 0) {
            int idx = static_cast<int>(av.poses_.size());
            av.poses_.push_back(PoseRef{fgnd, trans, aura});
            lastOffset = fgnd;
            lastPoseIndex = idx;
            return idx;
        }
        return lastPoseIndex;
    };

    bool done = false;
    while (!done) {
        u16 key;
        if (!rdU16(fp, key)) break; // EOF guard (original looped on while(TRUE))
        switch (key) {
        case kAK_NAME: {
            // Null-terminated string; consume it (we keep the filename instead).
            int c;
            while ((c = std::fgetc(fp)) != EOF && c != 0) {}
            break;
        }
        case kAK_STYLE: {
            u16 v; if (!rdU16(fp, v)) { done = true; }
            break;
        }
        case kAK_FLAGS: {
            u16 v; if (!rdU16(fp, v)) { done = true; break; }
            av.flags_ = static_cast<u8>(v);
            break;
        }
        case kAK_ICON: {
            u32 v; if (!rdU32(fp, v)) { done = true; } // icon fgnd offset
            break;
        }
        case kAK_NBODIES: {
            lastOffset = 0;
            lastPoseIndex = -1;
            u16 nBodies;
            if (!rdU16(fp, nBodies)) { done = true; break; }
            for (int i = 0; i < nBodies && !done; ++i) {
                u32 fgnd, trans, aura;
                if (!rdU32(fp, fgnd) || !rdU32(fp, trans) || !rdU32(fp, aura)) { done = true; break; }
                int poseIndex = registerPose(fgnd, trans, aura);
                i16 em; u8 inten; u16 fx, fy;
                if (!rdI16(fp, em) || !rdU8(fp, inten) || !rdU16(fp, fx) || !rdU16(fp, fy)) { done = true; break; }
                // 16 bytes padding.
                for (int k = 0; k < 16; ++k) { if (std::fgetc(fp) == EOF) { done = true; break; } }

                BodyRec rec;
                rec.poseIndex = poseIndex;
                rec.emotion = emotionToFloat(em);
                rec.intensity = static_cast<float>(inten) / 255.0f;
                rec.faceX = static_cast<u8>(fx);
                rec.faceY = static_cast<u8>(fy);
                av.bodies_.push_back(rec);
            }
            break;
        }
        case kAK_NFACES: {
            lastOffset = 0;
            lastPoseIndex = -1;
            u16 nFaces;
            if (!rdU16(fp, nFaces)) { done = true; break; }
            for (int i = 0; i < nFaces && !done; ++i) {
                u32 fgnd, trans, aura;
                if (!rdU32(fp, fgnd) || !rdU32(fp, trans) || !rdU32(fp, aura)) { done = true; break; }
                int poseIndex = registerPose(fgnd, trans, aura);
                i16 em; u8 inten; i16 xcx, ycx, dx, dy; u16 fx, fy;
                if (!rdI16(fp, em) || !rdU8(fp, inten) ||
                    !rdI16(fp, xcx) || !rdI16(fp, ycx) || !rdI16(fp, dx) || !rdI16(fp, dy) ||
                    !rdU16(fp, fx) || !rdU16(fp, fy)) { done = true; break; }
                for (int k = 0; k < 16; ++k) { if (std::fgetc(fp) == EOF) { done = true; break; } }
                FaceRec r;
                r.poseIndex = poseIndex;
                r.emotion = emotionToFloat(em);
                r.intensity = static_cast<float>(inten) / 255.0f;
                r.xCX = xcx; r.yCX = ycx; r.deltaXCX = dx; r.deltaYCX = dy;
                r.faceX = static_cast<u8>(fx); r.faceY = static_cast<u8>(fy);
                av.faces_.push_back(r);
            }
            break;
        }
        case kAK_NTORSOS: {
            lastOffset = 0;
            lastPoseIndex = -1;
            u16 nTorsos;
            if (!rdU16(fp, nTorsos)) { done = true; break; }
            for (int i = 0; i < nTorsos && !done; ++i) {
                u32 fgnd, trans, aura;
                if (!rdU32(fp, fgnd) || !rdU32(fp, trans) || !rdU32(fp, aura)) { done = true; break; }
                int poseIndex = registerPose(fgnd, trans, aura);
                i16 em; u8 inten; i16 xcx, ycx;
                if (!rdI16(fp, em) || !rdU8(fp, inten) || !rdI16(fp, xcx) || !rdI16(fp, ycx)) { done = true; break; }
                for (int k = 0; k < 16; ++k) { if (std::fgetc(fp) == EOF) { done = true; break; } }
                TorsoRec r;
                r.poseIndex = poseIndex;
                r.emotion = emotionToFloat(em);
                r.intensity = static_cast<float>(inten) / 255.0f;
                r.xCX = xcx; r.yCX = ycx;
                av.torsos_.push_back(r);
            }
            break;
        }
        case kAK_STARTDATA:
            done = true;
            break;
        default:
            // Unknown key with no known payload — cannot safely resync.
            done = true;
            break;
        }
    }

    std::fclose(fp);
    if (av.bodies_.empty() && av.faces_.empty()) return std::nullopt;
    return av;
}

int Avatar::neutralBodyIndex() const {
    for (int i = 0; i < static_cast<int>(bodies_.size()); ++i) {
        if (bodies_[i].emotion == 0.0f && bodies_[i].intensity == 0.0f) return i;
    }
    return bodies_.empty() ? -1 : 0;
}

Dib Avatar::loadDibAt(u32 offset) const {
    Dib dib;
    std::FILE* fp = std::fopen(path_.c_str(), "rb");
    if (!fp) return dib;
    if (std::fseek(fp, static_cast<long>(offset), SEEK_SET) == 0)
        dib.loadFromFile(fp);
    std::fclose(fp);
    return dib;
}

Dib Avatar::loadDrawing(int bodyIndex) const {
    if (bodyIndex < 0 || bodyIndex >= static_cast<int>(bodies_.size())) return Dib{};
    return loadDibAt(poses_[bodies_[bodyIndex].poseIndex].fgndOffset);
}

Dib Avatar::loadPoseDrawing(int poseIndex) const {
    if (poseIndex < 0 || poseIndex >= static_cast<int>(poses_.size())) return Dib{};
    return loadDibAt(poses_[poseIndex].fgndOffset);
}

Dib Avatar::loadPoseMask(int poseIndex) const {
    if (poseIndex < 0 || poseIndex >= static_cast<int>(poses_.size())) return Dib{};
    u32 off = poses_[poseIndex].transOffset;
    if (off == 0) return Dib{}; // no mask
    return loadDibAt(off);
}

int Avatar::neutralFaceIndex() const {
    for (int i = 0; i < static_cast<int>(faces_.size()); ++i)
        if (faces_[i].emotion == 0.0f && faces_[i].intensity == 0.0f) return i;
    return faces_.empty() ? -1 : 0;
}

int Avatar::neutralTorsoIndex() const {
    for (int i = 0; i < static_cast<int>(torsos_.size()); ++i)
        if (torsos_[i].emotion == 0.0f && torsos_[i].intensity == 0.0f) return i;
    return torsos_.empty() ? -1 : 0;
}

ComposedBody Avatar::composeNeutralBody(bool maskInsideIsHigh) const {
    ComposedBody out;

    if (!complex_) {
        // AT_SIMPLE: single part, no head offset.
        int bi = neutralBodyIndex();
        if (bi < 0) return out;
        int pose = bodies_[bi].poseIndex;
        Dib drawing = loadPoseDrawing(pose);
        if (!drawing.valid()) return out;
        Dib mask = loadPoseMask(pose);
        out.width = drawing.width();
        out.height = drawing.height();
        out.rgba.assign(static_cast<size_t>(out.width) * out.height * 4, 0);
        stampPart(out.rgba, out.width, out.height, 0, 0,
                  drawing, mask.valid() ? &mask : nullptr, maskInsideIsHigh, 0);
        return out;
    }

    // AT_COMPLEX: head + torso.
    int fi = neutralFaceIndex(), ti = neutralTorsoIndex();
    if (fi < 0 || ti < 0) return out;
    const FaceRec& fr = faces_[fi];
    const TorsoRec& tr = torsos_[ti];

    Dib headDraw = loadPoseDrawing(fr.poseIndex);
    Dib torsoDraw = loadPoseDrawing(tr.poseIndex);
    if (!headDraw.valid() || !torsoDraw.valid()) return out;
    Dib headMask = loadPoseMask(fr.poseIndex);
    Dib torsoMask = loadPoseMask(tr.poseIndex);

    // GetBodyBox offset math (CBodyDouble::GetBodyBox, bodycam.cpp).
    int xOffset = tr.xCX + fr.deltaXCX - fr.xCX;
    int yOffset = tr.yCX + fr.deltaYCX - fr.yCX;
    int left = std::min(0, xOffset);
    int top = std::min(0, yOffset);
    int right = std::max(torsoDraw.width(), xOffset + headDraw.width());
    int bottom = std::max(torsoDraw.height(), yOffset + headDraw.height());

    out.width = right - left;
    out.height = bottom - top;
    if (out.width <= 0 || out.height <= 0) { out.width = out.height = 0; return out; }
    out.rgba.assign(static_cast<size_t>(out.width) * out.height * 4, 0);

    int torsoX = 0 - left, torsoY = 0 - top;
    int headX = xOffset - left, headY = yOffset - top;

    auto stampTorso = [&]() {
        stampPart(out.rgba, out.width, out.height, torsoX, torsoY,
                  torsoDraw, torsoMask.valid() ? &torsoMask : nullptr, maskInsideIsHigh, 0);
    };
    auto stampHead = [&]() {
        stampPart(out.rgba, out.width, out.height, headX, headY,
                  headDraw, headMask.valid() ? &headMask : nullptr, maskInsideIsHigh, 0);
    };

    constexpr u8 kTorsoFirst = 4;
    if (flags_ & kTorsoFirst) { stampTorso(); stampHead(); }
    else { stampHead(); stampTorso(); }
    return out;
}

} // namespace comic
