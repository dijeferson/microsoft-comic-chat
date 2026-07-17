// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Minimal AT_SIMPLE .avb loader, ported from avatario.cpp (LoadAvatar /
// LoadBodyRecs / LoadBasics) and avatario.h key constants.

#include "comic_avatar.h"

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
        magic != kMagic || avType != kAT_SIMPLE) {
        std::fclose(fp);
        return std::nullopt; // MVP: AT_SIMPLE only
    }

    // Dedupe poses by foreground offset, mirroring the "ditto" logic.
    u32 lastOffset = 0;
    int lastPoseIndex = -1;

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
        case kAK_STYLE:
        case kAK_FLAGS: {
            u16 v; if (!rdU16(fp, v)) { done = true; }
            break;
        }
        case kAK_ICON: {
            u32 v; if (!rdU32(fp, v)) { done = true; } // icon fgnd offset
            break;
        }
        case kAK_NBODIES: {
            u16 nBodies;
            if (!rdU16(fp, nBodies)) { done = true; break; }
            for (int i = 0; i < nBodies && !done; ++i) {
                u32 fgnd, trans, aura;
                if (!rdU32(fp, fgnd) || !rdU32(fp, trans) || !rdU32(fp, aura)) { done = true; break; }
                int poseIndex;
                if (fgnd != lastOffset || lastPoseIndex < 0) {
                    poseIndex = static_cast<int>(av.poses_.size());
                    av.poses_.push_back(PoseRef{fgnd, trans, aura});
                    lastOffset = fgnd;
                    lastPoseIndex = poseIndex;
                } else {
                    poseIndex = lastPoseIndex; // ditto
                }
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
    if (av.bodies_.empty()) return std::nullopt;
    return av;
}

int Avatar::neutralBodyIndex() const {
    for (int i = 0; i < static_cast<int>(bodies_.size()); ++i) {
        if (bodies_[i].emotion == 0.0f && bodies_[i].intensity == 0.0f) return i;
    }
    return 0;
}

Dib Avatar::loadDrawing(int bodyIndex) const {
    Dib dib;
    if (bodyIndex < 0 || bodyIndex >= static_cast<int>(bodies_.size())) return dib;
    const PoseRef& pose = poses_[bodies_[bodyIndex].poseIndex];
    std::FILE* fp = std::fopen(path_.c_str(), "rb");
    if (!fp) return dib;
    if (std::fseek(fp, static_cast<long>(pose.fgndOffset), SEEK_SET) == 0)
        dib.loadFromFile(fp);
    std::fclose(fp);
    return dib;
}

} // namespace comic
