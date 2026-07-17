# AT_COMPLEX Avatars Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the macOS comic-only MVP (`v1.0-pre-mac/`) to render the 15 `AT_COMPLEX` characters (head+torso composition with 1-bit mask silhouettes), so all 22 original avatars work.

**Architecture:** Add `AT_COMPLEX` parsing to the portable `.avb` loader, load each pose's mask DIB alongside its drawing, and composite head+torso into a single premultiplied-RGBA body bitmap in `libcomic` (alpha from the mask, RGB from the drawing — the modern equivalent of the original's `MERGEPAINT`+`SRCAND` raster ops). The `IComicRenderer` seam and the Cocoa app are unchanged except for offering all characters; a body is still one image to the panel.

**Tech Stack:** C++17 (`libcomic`), Objective-C++ / Cocoa / CoreGraphics (`mac/`), `clang++`, `make`. No test framework — verification uses small headless assertion programs and PNG dumps, matching the MVP's established pattern (`test_art`, `render_panel`).

---

## Background: what the original does

From `avatar.h`, `avatario.cpp`, `bodycam.cpp` (`v1.0-pre-modern/`):

- An `AT_COMPLEX` avatar has **separate face and torso lists** (`AK_NFACES`, `AK_NTORSOS`), not a single body list. `FACEREC` on disk is 43 bytes, `TORSOREC` is 35 bytes (see layouts in Task 2).
- Each record holds three file offsets (`fgnd`/`trans`/`aura`) to standalone BMPs, an emotion index + intensity, and geometry. The visible drawing is `fgnd`; `trans` is the 1-bit silhouette **mask**; `aura` is an optional nimbus (out of scope here).
- `CBodyDouble::DrawBody` (`bodycam.cpp:446`) composites, in `TORSOFIRST`-flag order, each part as: draw the mask with `MERGEPAINT`, then the drawing with `SRCAND`. Positioning comes from `GetBodyBox` (`bodycam.cpp:533`):
  - `xOffset = torsoRec.xCX + faceRec.delta_xCX - faceRec.xCX`
  - `yOffset = torsoRec.yCX + faceRec.delta_yCX - faceRec.yCX`
  - `bitRect = union( torso at (0,0), head at (xOffset,yOffset) )`
- Avatar `m_flags` (`AK_FLAGS`) bits: `HEADMASK=1`, `TORSOMASK=2`, `TORSOFIRST=4`.

**Modern approach:** compose at native pixel resolution into one RGBA buffer.
For each part, alpha = mask silhouette (opaque inside the character outline,
including white skin/shirt; transparent outside), RGB = drawing. Draw parts in
`TORSOFIRST` order so the head correctly occludes the torso where they overlap.
This unifies with `AT_SIMPLE` (a single part, no head). The mask polarity
(which index is "inside") is **verified empirically** in Task 5, the same way the
1-bit drawing decode was validated in the MVP.

## File structure

- Modify `v1.0-pre-mac/libcomic/comic_avatar.h` — add `FaceRec`/`TorsoRec`, complex fields, `ComposedBody`, new public methods.
- Modify `v1.0-pre-mac/libcomic/comic_avatar.cpp` — parse `AT_COMPLEX`, load masks, compose bodies.
- Create `v1.0-pre-mac/libcomic/comic_compose.h` / `.cpp` — the pure RGBA part-compositor (mask+drawing → premultiplied RGBA), independently testable.
- Modify `v1.0-pre-mac/libcomic/test_art.cpp` — extend to exercise `composeBody` for any avatar.
- Modify `v1.0-pre-mac/mac/main.mm` — offer all 22 characters; use `composeBody`.
- Modify `v1.0-pre-mac/mac/render_panel.mm` — use `composeBody` so headless renders cover complex avatars.
- Modify `v1.0-pre-mac/Makefile` — add `comic_compose.cpp` to `CORE_SRC`.

---

## Task 1: Add the RGBA part-compositor (pure, testable)

A standalone function that stamps one decoded `Dib` (drawing) + optional mask
`Dib` into an RGBA canvas at a given offset, deriving alpha from the mask. Pure
C++, no OS, no avatar knowledge — so it can be unit-tested directly.

**Files:**
- Create: `v1.0-pre-mac/libcomic/comic_compose.h`
- Create: `v1.0-pre-mac/libcomic/comic_compose.cpp`
- Create: `v1.0-pre-mac/libcomic/test_compose.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_compose.h — composite a pose's drawing (+ optional 1-bit mask) into an
// RGBA canvas. Alpha comes from the mask silhouette (opaque inside the figure);
// with no mask, non-background pixels are opaque. Pure C++.

#ifndef COMIC_COMPOSE_H
#define COMIC_COMPOSE_H

#include <vector>

#include "comic_dib.h"
#include "comic_types.h"

namespace comic {

// A composed, ready-to-upload body bitmap (top-down RGBA, width*height*4).
struct ComposedBody {
    std::vector<u8> rgba;
    int width = 0;
    int height = 0;
    bool valid() const { return width > 0 && height > 0 && !rgba.empty(); }
};

// Stamp `drawing` into `canvas` (canvasW x canvasH, top-down RGBA) with its
// top-left at (destX,destY). If `mask` is non-null, a pixel is opaque where the
// mask marks the silhouette (see maskInsideIsHigh) and transparent elsewhere;
// if null, a pixel is opaque unless it equals `bgIndex` in the drawing.
// `maskInsideIsHigh`: true if the higher palette index (1) is "inside" the
// figure in the mask. RGB is always taken from the drawing's palette.
void stampPart(std::vector<u8>& canvas, int canvasW, int canvasH,
               int destX, int destY,
               const Dib& drawing, const Dib* mask,
               bool maskInsideIsHigh, int bgIndex);

} // namespace comic

#endif // COMIC_COMPOSE_H
```

- [ ] **Step 2: Write the failing test**

```cpp
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
    // Canvas 4x1, opaque-red fill to observe transparency.
    std::vector<u8> canvas(4 * 1 * 4, 0);

    // With an empty/invalid drawing, stampPart must be a no-op (no crash).
    Dib empty;
    stampPart(canvas, 4, 1, 0, 0, empty, nullptr, true, 0);
    for (u8 v : canvas) assert(v == 0);

    std::printf("test_compose OK\n");
    return 0;
}
```

- [ ] **Step 3: Run test to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_compose.cpp libcomic/comic_compose.cpp libcomic/comic_dib.cpp -o build/test_compose
```
Expected: FAIL — link error, `stampPart` undefined (comic_compose.cpp not yet implemented).

- [ ] **Step 4: Write the implementation**

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "comic_compose.h"

namespace comic {

void stampPart(std::vector<u8>& canvas, int canvasW, int canvasH,
               int destX, int destY,
               const Dib& drawing, const Dib* mask,
               bool maskInsideIsHigh, int bgIndex) {
    if (!drawing.valid()) return;
    int w = drawing.width();
    int h = drawing.height();
    for (int y = 0; y < h; ++y) {
        int cy = destY + y;
        if (cy < 0 || cy >= canvasH) continue;
        for (int x = 0; x < w; ++x) {
            int cx = destX + x;
            if (cx < 0 || cx >= canvasW) continue;

            bool opaque;
            if (mask && mask->valid() && x < mask->width() && y < mask->height()) {
                u8 m = mask->indexAt(x, y);
                bool high = (m != 0);
                opaque = maskInsideIsHigh ? high : !high;
            } else {
                opaque = drawing.indexAt(x, y) != bgIndex;
            }
            if (!opaque) continue;

            const RGBA& c = drawing.paletteEntry(drawing.indexAt(x, y));
            size_t o = (static_cast<size_t>(cy) * canvasW + cx) * 4;
            canvas[o + 0] = c.r;
            canvas[o + 1] = c.g;
            canvas[o + 2] = c.b;
            canvas[o + 3] = 255;
        }
    }
}

} // namespace comic
```

- [ ] **Step 5: Run test to verify it passes**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_compose.cpp libcomic/comic_compose.cpp libcomic/comic_dib.cpp -o build/test_compose && ./build/test_compose
```
Expected: PASS — prints `test_compose OK`.

- [ ] **Step 6: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_compose.h v1.0-pre-mac/libcomic/comic_compose.cpp v1.0-pre-mac/libcomic/test_compose.cpp
git commit -m "Add pure RGBA part-compositor for avatar poses"
```

---

## Task 2: Parse AT_COMPLEX records in the .avb loader

Extend `Avatar` to read the face/torso lists and the avatar flags. Keep the
existing `AT_SIMPLE` path working.

**Files:**
- Modify: `v1.0-pre-mac/libcomic/comic_avatar.h`
- Modify: `v1.0-pre-mac/libcomic/comic_avatar.cpp`

Record layouts on disk (from `avatario.cpp` `LoadFaceRecs`/`LoadTorsoRecs`):

```
FACEREC (43 bytes):  i32 fgndOff, i32 transOff, i32 auraOff,
                     i16 emotion, u8 intensity,
                     i16 xCX, i16 yCX, i16 delta_xCX, i16 delta_yCX,
                     i16 faceX, i16 faceY,          // stored i16, used as u8
                     16 bytes padding
TORSOREC (35 bytes): i32 fgndOff, i32 transOff, i32 auraOff,
                     i16 emotion, u8 intensity,
                     i16 xCX, i16 yCX,
                     16 bytes padding
```

- [ ] **Step 1: Add types + fields to the header**

In `comic_avatar.h`, after `struct BodyRec { ... };` add:

```cpp
// AT_COMPLEX head record (ported from FACEREC).
struct FaceRec {
    int poseIndex = 0;
    float emotion = 0.0f;
    float intensity = 0.0f;
    short xCX = 0, yCX = 0, deltaXCX = 0, deltaYCX = 0;
    u8 faceX = 0, faceY = 0;
};

// AT_COMPLEX torso record (ported from TORSOREC).
struct TorsoRec {
    int poseIndex = 0;
    float emotion = 0.0f;
    float intensity = 0.0f;
    short xCX = 0, yCX = 0;
};
```

In `class Avatar`, add to the public section:

```cpp
    bool isComplex() const { return complex_; }
    int faceCount() const { return static_cast<int>(faces_.size()); }
    int torsoCount() const { return static_cast<int>(torsos_.size()); }
    u8 flags() const { return flags_; }
```

and to the private section:

```cpp
    bool complex_ = false;
    u8 flags_ = 0;
    std::vector<FaceRec> faces_;
    std::vector<TorsoRec> torsos_;
    std::vector<PoseRef> masks_; // parallel to poses_: transOffset per pose
```

Also extend `PoseRef` usage: masks are already in `PoseRef.transOffset`; no new
field needed. Remove `masks_` if you prefer — but keeping `transOffset` in
`poses_` (already present) is enough. **Decision: do not add `masks_`; use the
existing `PoseRef.transOffset`.** Delete the `masks_` line above.

- [ ] **Step 2: Write a failing test (parse a real complex avatar)**

Append to `v1.0-pre-mac/libcomic/test_art.cpp` is not ideal (it has a `main`).
Instead create `v1.0-pre-mac/libcomic/test_complex.cpp`:

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
using namespace comic;
int main(int argc, char** argv) {
    // argv: <avatarDir> <complexName>
    auto av = Avatar::load(argv[1], argv[2]);
    assert(av && "complex avatar should load");
    assert(av->isComplex() && "should be AT_COMPLEX");
    assert(av->faceCount() > 0 && "has faces");
    assert(av->torsoCount() > 0 && "has torsos");
    std::printf("%s: complex=%d faces=%d torsos=%d flags=%u\n",
                argv[2], av->isComplex(), av->faceCount(), av->torsoCount(), av->flags());
    return 0;
}
```

- [ ] **Step 3: Run to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_complex.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp -o build/test_complex && ./build/test_complex ../v1.0-pre-modern/comicart/avatars mike
```
Expected: FAIL — `mike` currently returns `nullopt` (loader rejects non-`AT_SIMPLE`), so the first assert fails.

- [ ] **Step 4: Implement AT_COMPLEX parsing**

In `comic_avatar.cpp`, add the key/type constants near the others:

```cpp
constexpr int kAT_COMPLEX = 2;
constexpr int kAK_NFACES = 4;
constexpr int kAK_NTORSOS = 5;
```

In `Avatar::load`, replace the type guard

```cpp
        magic != kMagic || avType != kAT_SIMPLE) {
```

with

```cpp
        magic != kMagic || (avType != kAT_SIMPLE && avType != kAT_COMPLEX)) {
```

Set `av.complex_ = (avType == kAT_COMPLEX);` right after the guard passes.

The loader currently combines `AK_STYLE` and `AK_FLAGS` into one fall-through
case:

```cpp
        case kAK_STYLE:
        case kAK_FLAGS: {
            u16 v; if (!rdU16(fp, v)) { done = true; }
            break;
        }
```

**Split them into two separate cases** so `AK_FLAGS` captures its value while
`AK_STYLE` still just consumes its `u16`:

```cpp
        case kAK_STYLE: {
            u16 v; if (!rdU16(fp, v)) { done = true; }
            break;
        }
        case kAK_FLAGS: {
            u16 v; if (!rdU16(fp, v)) { done = true; break; }
            av.flags_ = static_cast<u8>(v);
            break;
        }
```

Add two new cases before `kAK_STARTDATA`, mirroring `avatario.cpp`. Both reuse
the pose-dedupe logic; factor a lambda at the top of the parse loop:

```cpp
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
```

Then refactor the existing `AK_NBODIES` block to call `registerPose(...)` for
its `fgnd/trans/aura`, and add:

```cpp
        case kAK_NFACES: {
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
```

Finally, relax the empty-check at the end of `load`:

```cpp
    if (av.bodies_.empty() && av.faces_.empty()) return std::nullopt;
```

- [ ] **Step 5: Run to verify it passes**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_complex.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp -o build/test_complex && ./build/test_complex ../v1.0-pre-modern/comicart/avatars mike
```
Expected: PASS — prints e.g. `mike: complex=1 faces=... torsos=... flags=...`.

- [ ] **Step 6: Verify AT_SIMPLE still parses**

Run:
```bash
cd v1.0-pre-mac && make test_art && ./build/test_art ../v1.0-pre-modern/comicart/avatars connor /tmp/connor_regress.png
```
Expected: PASS — still prints `loaded connor: 15 bodies` and writes the PNG.

- [ ] **Step 7: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_avatar.h v1.0-pre-mac/libcomic/comic_avatar.cpp v1.0-pre-mac/libcomic/test_complex.cpp
git commit -m "Parse AT_COMPLEX face/torso records in .avb loader"
```

---

## Task 3: Compose a body (unified simple + complex) → ComposedBody

Add `Avatar::composeBody()` returning a single premultiplied-RGBA bitmap, using
`stampPart` and the `GetBodyBox` offset math. For `AT_SIMPLE` it stamps one part;
for `AT_COMPLEX` it stamps torso+head in flag order into the union canvas.

**Files:**
- Modify: `v1.0-pre-mac/libcomic/comic_avatar.h`
- Modify: `v1.0-pre-mac/libcomic/comic_avatar.cpp`

- [ ] **Step 1: Declare the API in the header**

Add `#include "comic_compose.h"` to `comic_avatar.h`. In `class Avatar` public
section add:

```cpp
    // Neutral selection for complex avatars.
    int neutralFaceIndex() const;
    int neutralTorsoIndex() const;

    // Compose the neutral body into a single RGBA bitmap. Works for both
    // AT_SIMPLE (one part) and AT_COMPLEX (head+torso). Empty ComposedBody on
    // failure.
    ComposedBody composeNeutralBody(bool maskInsideIsHigh = true) const;
```

Add a private helper to load a pose's drawing + mask:

```cpp
    Dib loadPoseDrawing(int poseIndex) const;
    Dib loadPoseMask(int poseIndex) const;
```

- [ ] **Step 2: Write the failing test**

Create `v1.0-pre-mac/libcomic/test_body.cpp`:

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
#include "png_writer.h"
using namespace comic;
int main(int argc, char** argv) {
    // argv: <avatarDir> <name> <out.png>
    auto av = Avatar::load(argv[1], argv[2]);
    assert(av);
    ComposedBody body = av->composeNeutralBody(true);
    assert(body.valid() && "composed body must be non-empty");
    assert((int)body.rgba.size() == body.width * body.height * 4);
    bool wrote = writePng(argv[3], body.rgba, body.width, body.height);
    assert(wrote);
    std::printf("%s: composed %dx%d\n", argv[2], body.width, body.height);
    return 0;
}
```

- [ ] **Step 3: Run to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_body.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp libcomic/png_writer.cpp -lz -o build/test_body
```
Expected: FAIL — link error, `composeNeutralBody` / `neutralFaceIndex` undefined.

- [ ] **Step 4: Implement in `comic_avatar.cpp`**

Add the pose loaders (generalize the existing `loadDrawing`):

```cpp
Dib Avatar::loadPoseDrawing(int poseIndex) const {
    Dib dib;
    if (poseIndex < 0 || poseIndex >= static_cast<int>(poses_.size())) return dib;
    std::FILE* fp = std::fopen(path_.c_str(), "rb");
    if (!fp) return dib;
    if (std::fseek(fp, static_cast<long>(poses_[poseIndex].fgndOffset), SEEK_SET) == 0)
        dib.loadFromFile(fp);
    std::fclose(fp);
    return dib;
}

Dib Avatar::loadPoseMask(int poseIndex) const {
    Dib dib;
    if (poseIndex < 0 || poseIndex >= static_cast<int>(poses_.size())) return dib;
    u32 off = poses_[poseIndex].transOffset;
    if (off == 0) return dib; // no mask
    std::FILE* fp = std::fopen(path_.c_str(), "rb");
    if (!fp) return dib;
    if (std::fseek(fp, static_cast<long>(off), SEEK_SET) == 0)
        dib.loadFromFile(fp);
    std::fclose(fp);
    return dib;
}
```

Add neutral selectors:

```cpp
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
```

Add the compositor:

```cpp
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

    // GetBodyBox offset math (bodycam.cpp:533).
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
```

Add `#include <algorithm>` to `comic_avatar.cpp` if not present (for `std::min`/`std::max`).

- [ ] **Step 5: Run to verify it passes (simple + complex)**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_body.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp libcomic/png_writer.cpp -lz -o build/test_body
./build/test_body ../v1.0-pre-modern/comicart/avatars connor /tmp/body_connor.png
./build/test_body ../v1.0-pre-modern/comicart/avatars mike /tmp/body_mike.png
```
Expected: PASS both — prints composed dimensions; writes two PNGs.

- [ ] **Step 6: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_avatar.h v1.0-pre-mac/libcomic/comic_avatar.cpp v1.0-pre-mac/libcomic/test_body.cpp
git commit -m "Compose AT_SIMPLE + AT_COMPLEX bodies into a single RGBA bitmap"
```

---

## Task 4: Wire composeNeutralBody into the Makefile + apps

Replace the MVP's direct `loadDrawing` + opaque `toRGBA` path with
`composeNeutralBody`, and add `comic_compose.cpp` to the core sources so the app
and `render_panel` link it.

**Files:**
- Modify: `v1.0-pre-mac/Makefile`
- Modify: `v1.0-pre-mac/mac/main.mm`
- Modify: `v1.0-pre-mac/mac/render_panel.mm`

- [ ] **Step 1: Add comic_compose.cpp to CORE_SRC**

In `Makefile`, change:

```make
CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp
```
to
```make
CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp libcomic/comic_compose.cpp
```

- [ ] **Step 2: Update main.mm to use composeNeutralBody + all characters**

Replace the `kSimpleChars` array with all 22:

```cpp
static const char* kChars[] = {
    "connor", "glenda", "jordan", "pedagog", "rainbow", "tux", "waf",
    "anna", "armando", "bolo", "cro", "dan", "denise", "hugh", "lance",
    "lynnea", "margaret", "mike", "susan", "tiki", "tongtyed", "xeno"};
```

Update the picker loop to iterate `kChars`.

Replace the body of `-loadCharacter:` with:

```cpp
- (void)loadCharacter:(NSString*)name {
    auto av = comic::Avatar::load(_avatarDir, std::string(name.UTF8String));
    if (!av) { NSLog(@"could not load %@", name); return; }
    comic::ComposedBody body = av->composeNeutralBody(/*maskInsideIsHigh=*/true);
    if (!body.valid()) { NSLog(@"could not compose %@", name); return; }
    if (_comic.bodyImage) CGImageRelease(_comic.bodyImage);
    _comic.bodyImage = [self makeImageFromRGBA:body.rgba width:body.width height:body.height];
    _comic.bodyW = body.width;
    _comic.bodyH = body.height;
    [_comic setNeedsDisplay:YES];
}
```

Remove the now-unused `#include "comic_dib.h"` only if nothing else references it
(the compose header pulls it in transitively; leaving it is harmless).

- [ ] **Step 3: Update render_panel.mm similarly**

Replace the decode block:

```cpp
        comic::Dib dib = av->loadDrawing(av->neutralBodyIndex());
        if (!dib.valid()) { fprintf(stderr, "FAIL decode\n"); return 1; }
        std::vector<comic::u8> rgba = dib.toRGBA(-1);
        CGImageRef body = MakeImage(rgba, dib.width(), dib.height());
```
with:
```cpp
        comic::ComposedBody cb = av->composeNeutralBody(/*maskInsideIsHigh=*/true);
        if (!cb.valid()) { fprintf(stderr, "FAIL compose\n"); return 1; }
        CGImageRef body = MakeImage(cb.rgba, cb.width, cb.height);
```
and update the `PanelBody` fill to use `cb.width` / `cb.height`. Add
`#include "comic_compose.h"`.

- [ ] **Step 4: Build everything**

Run:
```bash
cd v1.0-pre-mac && make clean && make app render_panel test_art
```
Expected: all link with no errors.

- [ ] **Step 5: Commit**

```bash
git add v1.0-pre-mac/Makefile v1.0-pre-mac/mac/main.mm v1.0-pre-mac/mac/render_panel.mm
git commit -m "Wire composeNeutralBody into app + render_panel; offer all 22 characters"
```

---

## Task 5: Verify mask polarity empirically and render complex panels

The `maskInsideIsHigh` flag is a hypothesis. Render a complex character both
ways and pick the correct one by inspection (head opaque over torso, no
see-through skin, clean silhouette) — the same empirical method used for the
1-bit drawing decode in the MVP.

**Files:**
- Modify (if needed): default arg in `main.mm` / `render_panel.mm` calls.

- [ ] **Step 1: Render a complex character both polarities**

Run:
```bash
cd v1.0-pre-mac
./build/render_panel ../v1.0-pre-modern/comicart/avatars mike "Testing complex avatars." /tmp/mike_high.png
```
Then temporarily flip the default to `false` (edit the `composeNeutralBody(...)`
call in `render_panel.mm`, rebuild `make render_panel`) and render:
```bash
./build/render_panel ../v1.0-pre-modern/comicart/avatars mike "Testing complex avatars." /tmp/mike_low.png
```

- [ ] **Step 2: Inspect both PNGs**

Open `/tmp/mike_high.png` and `/tmp/mike_low.png`. The CORRECT one shows Mike as
a solid figure: the head sits opaquely over the torso, the face interior is
filled (not transparent showing torso lines through it), and the outline is
clean with no inverted/holed silhouette. Keep whichever polarity is correct as
the default in BOTH `main.mm` and `render_panel.mm`.

- [ ] **Step 3: Render several complex characters for confidence**

Run (with the chosen polarity):
```bash
cd v1.0-pre-mac
for c in mike anna dan susan xeno tiki; do
  ./build/render_panel ../v1.0-pre-modern/comicart/avatars $c "Hi, I'm $c!" /tmp/panel_$c.png
done
```
Expected: each writes a PNG showing a correctly-composited character with a
speech balloon. Inspect them; all should look like coherent figures.

- [ ] **Step 4: Regression-check simple characters still render**

Run:
```bash
cd v1.0-pre-mac
./build/render_panel ../v1.0-pre-modern/comicart/avatars connor "Still here." /tmp/panel_connor2.png
./build/render_panel ../v1.0-pre-modern/comicart/avatars tux "Me too." /tmp/panel_tux2.png
```
Expected: unchanged from the MVP — clean single-part figures.

- [ ] **Step 5: Commit the confirmed polarity**

```bash
git add v1.0-pre-mac/mac/main.mm v1.0-pre-mac/mac/render_panel.mm
git commit -m "Confirm mask polarity for AT_COMPLEX compositing"
```

---

## Task 6: Update docs

**Files:**
- Modify: `v1.0-pre-mac/README.md`
- Add: `docs/img/mac-mvp-complex.png` (a chosen complex render, e.g. mike)

- [ ] **Step 1: Refresh README scope**

In `v1.0-pre-mac/README.md`, move "AT_COMPLEX characters" and "1-bit mask/aura
compositing" out of the "deferred" list into "What works", noting all 22
characters now render and the head+torso mask compositing is implemented (aura
still deferred). Update the character list line.

- [ ] **Step 2: Add a complex example image**

```bash
cp /tmp/panel_mike.png docs/img/mac-mvp-complex.png
```
Reference it in the README under a short "Complex characters" note.

- [ ] **Step 3: Commit**

```bash
git add v1.0-pre-mac/README.md docs/img/mac-mvp-complex.png
git commit -m "Document AT_COMPLEX support in the macOS MVP"
```

---

## Notes for the implementer

- **No emotion yet:** we deliberately compose the *neutral* face+torso. Text →
  emotion selection is a separate plan; do not add it here (YAGNI).
- **Aura/nimbus** (`auraOffset`, `MERGEPAINT` nimbus) is out of scope — leave
  `auraOffset` parsed but unused.
- **Coordinate space:** `composeNeutralBody` works in native top-down pixel
  space; the `Panel` already scales the single body image to fit, so no TWIP or
  Y-flip concerns arise inside libcomic (that stays at the renderer seam).
- **If a complex avatar fails to compose** (e.g. an unexpected DIB bit depth in a
  mask), `loadFromFile` returns invalid and `composeNeutralBody` yields an empty
  body; the app logs and shows no figure rather than crashing. If that happens,
  hexdump the mask BMP header (as in the MVP's `probe2`) and extend
  `comic_dib.cpp` decoding — likely RLE4 or 1bpp masks (source-map risk #3).
