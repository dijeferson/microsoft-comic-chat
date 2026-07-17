# Text → Emotion/Gesture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make typed text drive a character's expression/pose in the macOS Comic Chat port (`v1.0-pre-mac/`), instead of always rendering the neutral pose — a faithful reproduction of Comic Chat's rule-based expert system.

**Architecture:** Add a portable rule engine (`comic_semantics`) that maps text → a priority-ranked set of emotion options (ported from `textpose.cpp` `GetEmotionsFromString` + the original resource rule table). Add emotion→pose selection to `Avatar` (ported from `avatar.cpp` `GetBodyFromEmotion(CEmotionOpts&)` + the angular nearest-match helpers), reusing the angle math from the original `vector2d.cpp`. Then `Avatar::composeBodyForText(text)` runs rules → selects poses → composites via the existing Task-1..5 compositor. `composeNeutralBody` remains the no-match/empty path. Wire the app + render_panel to pass their text through.

**Tech Stack:** C++17 (`libcomic`, namespace `comic`, fixed-width types, no OS deps), Objective-C++/Cocoa (`mac/`), `clang++`, hand-written `Makefile`. Tests are standalone `main()`+`assert` programs compiled ad-hoc with clang++ (matching `test_compose.cpp` etc.); they are NOT wired into the Makefile.

---

## Background: how the original works

From `v1.0-pre-modern/textpose.cpp`, `avatar.cpp`, `vector2d.cpp` and the rule table in `v2.5-beta-1-modern/chat.rc`:

- **Emotion wheel:** 8 emotions are angles `k*2*PI/8` (HAPPY=0, COY, BORED, SCARED, SAD, ANGRY, SHOUT, LAUGH), NEUTRAL=0.0, plus gesture "emotions" WAVE/POINTOTHER/POINTSELF/… stored as sentinel floats `1001.0..1008.0` (> 2π, so angular matching is skipped for them). These constants already exist in `comic_avatar.cpp`'s `emotionToFloat` table.
- **Rules** are composite strings of `Function(arg);strength` entries. The functions: `AllCaps` (whole string is caps), `FindString`/`FindString*` (substring, `*`=case-insensitive), `CheckWord`/`CheckWord*` (whole-word), `CheckStart`/`CheckStart*` (sentence-initial). Each rule maps to one emotion. The exact rule table (from chat.rc):

  | Emotion | Rules |
  |---------|-------|
  | SHOUT | `AllCaps("");9` , `FindString("!!!");9` |
  | LAUGH | `CheckWord*("ROTFL");11` , `CheckWord*("LOL");11` , `FindString*("HEHE");11` |
  | HAPPY | `FindString(":)");10` , `FindString(":-)");10` |
  | SAD | `FindString(":(");10` , `FindString(":-(");10` |
  | POINTOTHER | `CheckStart*("You");4` , `CheckWord*("are you");8` , `CheckWord*("will you");8` , `CheckWord*("did you");8` , `CheckWord*("aren't you");8` , `CheckWord*("don't you");8` |
  | POINTSELF | `CheckStart*("I");3` , `CheckWord*("i'm");7` , `CheckWord*("i will");7` , `CheckWord*("i'll");7` , `CheckWord*("i am");7` |
  | WAVE | `CheckStart*("Hi");2` , `CheckStart*("Bye");3` , `CheckStart*("Hello");5` , `CheckStart*("Welcome");5` , `CheckStart*("Howdy");5` |
  | COY | `FindString(";-)");10` , `FindString(";)");10` |
  | ANGRY / SCARED / BORED | *(empty — no triggers in the original)* |

- **Accumulation** (`CEmotionOpts::Add`, avatar.cpp:714): options are keyed by emotion. Default flag `OVERRIDEBYPRIORITY`: if the emotion is already present, keep the higher priority (and take that entry's intensity); otherwise append (cap `MAXEMOPTS=10`). All rule matches here call `Add(emotion, 1.0, strength)` — intensity always 1.0.
- **Matching** (`GetEmotionsFromString`, textpose.cpp:267): AllCaps first (if `capsStrength` set and `CheckForUppers`), then general/`FindString` rules, then word/`CheckWord` rules, then per-sentence `CheckStart` rules (iterating sentence starts via `.!?`). Case-insensitive variants match against a lowercased copy.
- **Selection** (`GetBodyFromEmotion(CEmotionOpts&)`, avatar.cpp:379 simple / :347 complex): repeatedly take the highest-priority remaining option, find the nearest pose by `fabs(subtract_angles(pose.emotion, opt.emotion))` (with intensity as tiebreak); the first option that resolves to a valid pose wins; else neutral. Complex avatars resolve a face and a torso separately.
- **Angle math** (`vector2d.cpp`): `value_to_angle` normalizes to `(-PI, PI]`; `subtract_angles(a,b) = value_to_angle(a-b)`. `PI` is `3.14159` (deliberately imprecise — preserve it).

## File structure

- Create `v1.0-pre-mac/libcomic/comic_angles.h` — `PI`, `value_to_angle`, `subtract_angles` (tiny, header-only inline).
- Create `v1.0-pre-mac/libcomic/comic_semantics.{h,cpp}` — `EmotionOpts` + rule engine (`emotionsFromText`).
- Create `v1.0-pre-mac/libcomic/test_semantics.cpp` — rule-match assertions.
- Modify `v1.0-pre-mac/libcomic/comic_avatar.{h,cpp}` — emotion→pose selection + `composeBodyForText`.
- Create `v1.0-pre-mac/libcomic/test_emotion_pose.cpp` — selection + compose-for-text assertions.
- Modify `v1.0-pre-mac/mac/main.mm` and `mac/render_panel.mm` — feed text through `composeBodyForText`.
- Modify `v1.0-pre-mac/Makefile` — add `comic_semantics.cpp` to `CORE_SRC`.

The existing `comic_avatar.h` already exposes: `bool isComplex()`, `int bodyCount()`, `const BodyRec& body(int)`, `int faceCount()/torsoCount()`, and privately `std::vector<BodyRec> bodies_`, `std::vector<FaceRec> faces_`, `std::vector<TorsoRec> torsos_`, plus `composeNeutralBody(bool)` and private `loadDibAt/loadPoseDrawing/loadPoseMask`. `BodyRec/FaceRec/TorsoRec` each carry `float emotion; float intensity; int poseIndex;`. `ComposedBody` (in comic_compose.h) is `{ std::vector<u8> rgba; int width,height; bool valid(); }`.

---

## Task 1: Angle helpers

**Files:**
- Create: `v1.0-pre-mac/libcomic/comic_angles.h`
- Create: `v1.0-pre-mac/libcomic/test_angles.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_angles.h — angle normalization from the original vector2d.cpp. PI is
// deliberately the original's imprecise 3.14159 (pose-matching parity).

#ifndef COMIC_ANGLES_H
#define COMIC_ANGLES_H

namespace comic {

constexpr double kPI = 3.14159;

// Normalize an angle to (-PI, PI]. Ported from vector2d.cpp value_to_angle.
inline double valueToAngle(double value) {
    if (value > -kPI && value <= kPI) return value;
    double temp = value / (2 * kPI);
    temp = (temp - (int)temp) * 2 * kPI; // retain fractional component
    if (temp > kPI) return temp - 2 * kPI;
    else if (temp <= -kPI) return temp + 2 * kPI;
    else return temp;
}

// Ported from vector2d.cpp subtract_angles.
inline double subtractAngles(double a, double b) { return valueToAngle(a - b); }

} // namespace comic

#endif // COMIC_ANGLES_H
```

- [ ] **Step 2: Write the failing test** (`test_angles.cpp`)

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cmath>
#include <cstdio>
#include "comic_angles.h"
using namespace comic;
int main() {
    // Within range: identity.
    assert(std::fabs(valueToAngle(1.0) - 1.0) < 1e-9);
    // subtract_angles wraps: near +PI minus near -PI ~= 0 (not 2PI).
    double d = subtractAngles(kPI - 0.01, -(kPI - 0.01));
    assert(std::fabs(d) < 0.05); // wrapped to ~ -0.02, magnitude small
    // Opposite points on the wheel are ~PI apart.
    assert(std::fabs(std::fabs(subtractAngles(0.0, kPI)) - kPI) < 1e-6);
    std::printf("test_angles OK\n");
    return 0;
}
```

- [ ] **Step 3: Run to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_angles.cpp -o build/test_angles
```
Expected: FAIL — `comic_angles.h` not found (file not created yet). Once the header exists this compiles; run it in Step 4.

- [ ] **Step 4: Create the header (Step 1 content), then run to verify it passes**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_angles.cpp -o build/test_angles && ./build/test_angles
```
Expected: PASS — prints `test_angles OK`.

- [ ] **Step 5: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_angles.h v1.0-pre-mac/libcomic/test_angles.cpp
git commit -m "Add angle-normalization helpers (ported from vector2d.cpp)"
```

---

## Task 2: Rule engine (text → EmotionOpts)

**Files:**
- Create: `v1.0-pre-mac/libcomic/comic_semantics.h`
- Create: `v1.0-pre-mac/libcomic/comic_semantics.cpp`
- Create: `v1.0-pre-mac/libcomic/test_semantics.cpp`

- [ ] **Step 1: Write the header**

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_semantics.h — Comic Chat's text→emotion expert system, ported from
// textpose.cpp (GetEmotionsFromString + the chat.rc rule table). Produces a
// priority-ranked set of emotion options for a line of text.

#ifndef COMIC_SEMANTICS_H
#define COMIC_SEMANTICS_H

#include <string>
#include <vector>

namespace comic {

// One weighted emotion candidate. `emotion` is an emotion-wheel angle or a
// gesture sentinel (see comic_avatar emotionToFloat / EM_* values).
struct EmotionOpt {
    float emotion = 0.0f;
    float intensity = 1.0f;
    int priority = 0;
};

// Accumulator mirroring CEmotionOpts (OVERRIDEBYPRIORITY semantics), capped.
class EmotionOpts {
public:
    static constexpr int kMax = 10;
    // Keyed by emotion: if present, keep the higher priority (+ its intensity);
    // else append until full.
    void add(float emotion, float intensity, int priority);
    int count() const { return static_cast<int>(opts_.size()); }
    const EmotionOpt& at(int i) const { return opts_[i]; }
    std::vector<EmotionOpt>& items() { return opts_; }
private:
    std::vector<EmotionOpt> opts_;
};

// Run the fixed rule table over `text` and return the ranked options.
EmotionOpts emotionsFromText(const std::string& text);

} // namespace comic

#endif // COMIC_SEMANTICS_H
```

- [ ] **Step 2: Write the failing test** (`test_semantics.cpp`)

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_semantics.h"
#include "comic_avatar.h" // for EM_* float values via emotionToFloat parity
using namespace comic;

// Emotion-wheel angle helpers (mirror comic_avatar emotionToFloat table).
static const double PI = 3.14159265358979323846;
static float EM(int k) { return float(k * 2 * PI / 8); } // k: HAPPY=0..LAUGH=7
static const float EM_HAPPY = EM(0), EM_SHOUT = EM(6), EM_LAUGH = EM(7), EM_SAD = EM(4), EM_COY = EM(1);
static const float EM_WAVE = 1001.0f, EM_POINTSELF = 1003.0f, EM_POINTOTHER = 1002.0f;

static bool has(EmotionOpts& o, float emotion, int minPriority) {
    for (auto& e : o.items()) if (e.emotion == emotion && e.priority >= minPriority) return true;
    return false;
}

int main() {
    { auto o = emotionsFromText("HELLO!!!"); assert(has(o, EM_SHOUT, 9)); }      // AllCaps + !!!
    { auto o = emotionsFromText("that is funny :)"); assert(has(o, EM_HAPPY, 10)); }
    { auto o = emotionsFromText("oh no :("); assert(has(o, EM_SAD, 10)); }
    { auto o = emotionsFromText("lol that rocks"); assert(has(o, EM_LAUGH, 11)); }
    { auto o = emotionsFromText("Hello there"); assert(has(o, EM_WAVE, 5)); }    // CheckStart Hello
    { auto o = emotionsFromText("I think so"); assert(has(o, EM_POINTSELF, 3)); }
    { auto o = emotionsFromText("You are great"); assert(has(o, EM_POINTOTHER, 4)); }
    { auto o = emotionsFromText(";)"); assert(has(o, EM_COY, 10)); }
    { auto o = emotionsFromText("just some plain text"); assert(o.count() == 0); }
    std::printf("test_semantics OK\n");
    return 0;
}
```

- [ ] **Step 3: Run to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_semantics.cpp libcomic/comic_semantics.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp -o build/test_semantics
```
Expected: FAIL — link error, `emotionsFromText`/`EmotionOpts::add` undefined.

- [ ] **Step 4: Implement `comic_semantics.cpp`**

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Ported from textpose.cpp: the fixed rule table + GetEmotionsFromString.

#include "comic_semantics.h"

#include <cctype>
#include <cmath>

namespace comic {

namespace {

const double kPI = 3.14159265358979323846;
// Emotion-wheel angles (k*2PI/8) and gesture sentinels, matching comic_avatar.
float wheel(int k) { return static_cast<float>(k * 2 * kPI / 8); }
const float EM_HAPPY = wheel(0), EM_COY = wheel(1), EM_SAD = wheel(4),
            EM_SHOUT = wheel(6), EM_LAUGH = wheel(7);
const float EM_WAVE = 1001.0f, EM_POINTOTHER = 1002.0f, EM_POINTSELF = 1003.0f;

std::string toLower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

// Whole string is uppercase with >1 upper char (CheckForUppers).
bool checkForUppers(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) {
        if (std::islower(c)) return false;
        if (std::isupper(c)) ++n;
    }
    return n > 1;
}

// Whole-word substring match (CheckWord): substr starts a word and ends a word.
bool checkWord(const std::string& buff, const std::string& sub) {
    if (sub.empty()) return false;
    size_t pos = 0;
    while ((pos = buff.find(sub, pos)) != std::string::npos) {
        bool startOk = (pos == 0) || std::isspace(static_cast<unsigned char>(buff[pos - 1]));
        size_t after = pos + sub.size();
        bool endOk = (after >= buff.size()) ||
                     std::isspace(static_cast<unsigned char>(buff[after])) ||
                     std::ispunct(static_cast<unsigned char>(buff[after]));
        if (startOk && endOk) return true;
        ++pos;
    }
    return false;
}

// Sentence-initial compare (StartCompare2): substr at `sent`, not followed by alnum.
bool startCompare(const std::string& sent, const std::string& sub) {
    if (sent.size() < sub.size()) return false;
    if (sent.compare(0, sub.size(), sub) != 0) return false;
    if (sent.size() == sub.size()) return true;
    return !std::isalnum(static_cast<unsigned char>(sent[sub.size()]));
}

} // namespace

void EmotionOpts::add(float emotion, float intensity, int priority) {
    for (auto& e : opts_) {
        if (e.emotion == emotion) { // OVERRIDEBYPRIORITY
            if (e.priority < priority) { e.priority = priority; e.intensity = intensity; }
            return;
        }
    }
    if (static_cast<int>(opts_.size()) >= kMax) return;
    opts_.push_back(EmotionOpt{emotion, intensity, priority});
}

EmotionOpts emotionsFromText(const std::string& text) {
    EmotionOpts o;
    const std::string lower = toLower(text);

    // AllCaps -> SHOUT strength 9.
    if (checkForUppers(text)) o.add(EM_SHOUT, 1.0f, 9);

    // FindString rules (substring). Case-sensitive ones use text; * use lower.
    if (text.find("!!!") != std::string::npos) o.add(EM_SHOUT, 1.0f, 9);
    if (text.find(":)") != std::string::npos) o.add(EM_HAPPY, 1.0f, 10);
    if (text.find(":-)") != std::string::npos) o.add(EM_HAPPY, 1.0f, 10);
    if (text.find(":(") != std::string::npos) o.add(EM_SAD, 1.0f, 10);
    if (text.find(":-(") != std::string::npos) o.add(EM_SAD, 1.0f, 10);
    if (text.find(";-)") != std::string::npos) o.add(EM_COY, 1.0f, 10);
    if (text.find(";)") != std::string::npos) o.add(EM_COY, 1.0f, 10);
    if (lower.find("hehe") != std::string::npos) o.add(EM_LAUGH, 1.0f, 11);

    // CheckWord* rules (case-insensitive whole word) -> lower.
    if (checkWord(lower, "rotfl")) o.add(EM_LAUGH, 1.0f, 11);
    if (checkWord(lower, "lol")) o.add(EM_LAUGH, 1.0f, 11);
    if (checkWord(lower, "are you")) o.add(EM_POINTOTHER, 1.0f, 8);
    if (checkWord(lower, "will you")) o.add(EM_POINTOTHER, 1.0f, 8);
    if (checkWord(lower, "did you")) o.add(EM_POINTOTHER, 1.0f, 8);
    if (checkWord(lower, "aren't you")) o.add(EM_POINTOTHER, 1.0f, 8);
    if (checkWord(lower, "don't you")) o.add(EM_POINTOTHER, 1.0f, 8);
    if (checkWord(lower, "i'm")) o.add(EM_POINTSELF, 1.0f, 7);
    if (checkWord(lower, "i will")) o.add(EM_POINTSELF, 1.0f, 7);
    if (checkWord(lower, "i'll")) o.add(EM_POINTSELF, 1.0f, 7);
    if (checkWord(lower, "i am")) o.add(EM_POINTSELF, 1.0f, 7);

    // CheckStart* rules: iterate sentence starts (split on .!?), match lower.
    {
        const std::string term = ".!?";
        size_t i = 0;
        const std::string& s = lower;
        // first non-space
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        while (i < s.size()) {
            std::string rest = s.substr(i);
            if (startCompare(rest, "you")) o.add(EM_POINTOTHER, 1.0f, 4);
            if (startCompare(rest, "i")) o.add(EM_POINTSELF, 1.0f, 3);
            if (startCompare(rest, "hi")) o.add(EM_WAVE, 1.0f, 2);
            if (startCompare(rest, "bye")) o.add(EM_WAVE, 1.0f, 3);
            if (startCompare(rest, "hello")) o.add(EM_WAVE, 1.0f, 5);
            if (startCompare(rest, "welcome")) o.add(EM_WAVE, 1.0f, 5);
            if (startCompare(rest, "howdy")) o.add(EM_WAVE, 1.0f, 5);
            // advance to next sentence start
            size_t t = s.find_first_of(term, i);
            if (t == std::string::npos) break;
            i = t;
            while (i < s.size() && (std::ispunct(static_cast<unsigned char>(s[i])) ||
                                    std::isspace(static_cast<unsigned char>(s[i])))) ++i;
        }
    }

    return o;
}

} // namespace comic
```

- [ ] **Step 5: Run to verify it passes**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_semantics.cpp libcomic/comic_semantics.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp -o build/test_semantics && ./build/test_semantics
```
Expected: PASS — prints `test_semantics OK`.

- [ ] **Step 6: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_semantics.h v1.0-pre-mac/libcomic/comic_semantics.cpp v1.0-pre-mac/libcomic/test_semantics.cpp
git commit -m "Add text->emotion rule engine (ported from textpose.cpp)"
```

---

## Task 3: Emotion→pose selection + composeBodyForText

**Files:**
- Modify: `v1.0-pre-mac/libcomic/comic_avatar.h`
- Modify: `v1.0-pre-mac/libcomic/comic_avatar.cpp`
- Create: `v1.0-pre-mac/libcomic/test_emotion_pose.cpp`

- [ ] **Step 1: Declare API in the header**

Add near the top includes of `comic_avatar.h`: `#include "comic_semantics.h"`.

In `class Avatar` public section, add:

```cpp
    // Emotion→pose selection (ported from avatar.cpp GetBodyFromEmotion).
    // Simple: returns a body index (-1 if none). Complex: fills face+torso
    // indices (each -1 if none).
    int bodyIndexForEmotion(float emotion, float intensity) const;
    void faceTorsoForEmotion(float emotion, float intensity, int& faceIndex, int& torsoIndex) const;

    // Compose the pose selected by running the emotion rules over `text`.
    // Falls back to the neutral body when no rule matches or no pose resolves.
    ComposedBody composeBodyForText(const std::string& text, bool maskInsideIsHigh = true) const;
```

Add a private helper to compose from explicit indices (refactored out of
composeNeutralBody so both share one path):

```cpp
    ComposedBody composeFromIndices(int bodyIndex, int faceIndex, int torsoIndex,
                                    bool maskInsideIsHigh) const;
```

- [ ] **Step 2: Write the failing test** (`test_emotion_pose.cpp`)

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_avatar.h"
using namespace comic;
int main(int argc, char** argv) {
    // argv: <avatarDir> <simpleName> <complexName>
    if (argc != 4) { std::fprintf(stderr, "usage: %s <dir> <simple> <complex>\n", argv[0]); return 2; }
    const std::string dir = argv[1];

    // Simple avatar: a shout line must resolve to SOME body and compose.
    auto s = Avatar::load(dir, argv[2]);
    assert(s && !s->isComplex());
    ComposedBody shout = s->composeBodyForText("HELLO!!!");
    assert(shout.valid());
    // Plain text falls back to neutral, still valid.
    ComposedBody plain = s->composeBodyForText("just text here");
    assert(plain.valid());

    // Complex avatar: compose-for-text yields a valid body too.
    auto c = Avatar::load(dir, argv[3]);
    assert(c && c->isComplex());
    ComposedBody happy = c->composeBodyForText("nice one :)");
    assert(happy.valid());

    std::printf("emotion_pose OK: simple shout %dx%d, complex happy %dx%d\n",
                shout.width, shout.height, happy.width, happy.height);
    return 0;
}
```

- [ ] **Step 3: Run to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_emotion_pose.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp libcomic/comic_semantics.cpp -o build/test_emotion_pose
```
Expected: FAIL — link error, `composeBodyForText`/`bodyIndexForEmotion` undefined.

- [ ] **Step 4: Implement in `comic_avatar.cpp`**

Add `#include "comic_angles.h"` near the top includes.

Add the selection functions (ported from avatar.cpp GetBodyIndexFromEmotion /
GetHeadAndBodyFromEmotion — note the `emotion <= 2*PI` guard: angular match for
wheel emotions, exact-match for gesture sentinels):

```cpp
int Avatar::bodyIndexForEmotion(float emotion, float intensity) const {
    double nearestAngle = 3 * kPI;
    double intensityOfNearest = 2.0;
    int bIndex = -1;
    if (emotion <= 2 * kPI) {
        for (int i = 0; i < static_cast<int>(bodies_.size()); ++i) {
            double thisAngle = std::fabs(subtractAngles(bodies_[i].emotion, emotion));
            if (thisAngle <= nearestAngle) {
                double di = std::fabs(intensity - bodies_[i].intensity);
                if (thisAngle == nearestAngle && di >= intensityOfNearest) continue;
                nearestAngle = thisAngle;
                intensityOfNearest = di;
                bIndex = i;
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(bodies_.size()); ++i)
            if (bodies_[i].emotion == emotion) { bIndex = i; break; }
    }
    return bIndex;
}

void Avatar::faceTorsoForEmotion(float emotion, float intensity,
                                 int& faceIndex, int& torsoIndex) const {
    double nearestAngle = 3 * kPI;
    double intensityOfNearest = 2.0;
    faceIndex = torsoIndex = -1;
    if (emotion <= 2 * kPI) {
        for (int i = 0; i < static_cast<int>(faces_.size()); ++i) {
            double thisAngle = std::fabs(subtractAngles(faces_[i].emotion, emotion));
            if (thisAngle <= nearestAngle) {
                double di = std::fabs(intensity - faces_[i].intensity);
                if (thisAngle == nearestAngle && di >= intensityOfNearest) continue;
                nearestAngle = thisAngle;
                intensityOfNearest = di;
                faceIndex = i;
            }
        }
    } else {
        for (int i = 0; i < static_cast<int>(torsos_.size()); ++i)
            if (torsos_[i].emotion == emotion) { torsoIndex = i; break; }
    }
}
```

Add `#include <cmath>` and `#include <algorithm>` if not already present (algorithm is already there from Task 3 of the prior plan).

Refactor `composeNeutralBody` to delegate to a shared `composeFromIndices`, and
add `composeBodyForText`. Replace the existing `composeNeutralBody` body with:

```cpp
ComposedBody Avatar::composeFromIndices(int bodyIndex, int faceIndex, int torsoIndex,
                                        bool maskInsideIsHigh) const {
    ComposedBody out;
    if (!complex_) {
        if (bodyIndex < 0) return out;
        int pose = bodies_[bodyIndex].poseIndex;
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
    if (faceIndex < 0 || torsoIndex < 0) return out;
    const FaceRec& fr = faces_[faceIndex];
    const TorsoRec& tr = torsos_[torsoIndex];
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

ComposedBody Avatar::composeNeutralBody(bool maskInsideIsHigh) const {
    if (!complex_) return composeFromIndices(neutralBodyIndex(), -1, -1, maskInsideIsHigh);
    return composeFromIndices(-1, neutralFaceIndex(), neutralTorsoIndex(), maskInsideIsHigh);
}

ComposedBody Avatar::composeBodyForText(const std::string& text, bool maskInsideIsHigh) const {
    EmotionOpts opts = emotionsFromText(text);
    // Walk options highest-priority-first; first that resolves to a pose wins
    // (mirrors GetBodyFromEmotion). Fall back to neutral.
    // Work on a copy of priorities we can zero out.
    std::vector<EmotionOpt> items = opts.items();
    if (!complex_) {
        int found = -1;
        while (true) {
            int best = -1, bestPri = 0;
            for (int i = 0; i < static_cast<int>(items.size()); ++i)
                if (items[i].priority > bestPri) { bestPri = items[i].priority; best = i; }
            if (best < 0) break;
            int bi = bodyIndexForEmotion(items[best].emotion, items[best].intensity);
            items[best].priority = 0;
            if (bi >= 0) { found = bi; break; }
        }
        if (found < 0) return composeNeutralBody(maskInsideIsHigh);
        return composeFromIndices(found, -1, -1, maskInsideIsHigh);
    }
    int foundF = -1, foundT = -1;
    while (true) {
        int best = -1, bestPri = 0;
        for (int i = 0; i < static_cast<int>(items.size()); ++i)
            if (items[i].priority > bestPri) { bestPri = items[i].priority; best = i; }
        if (best < 0) break;
        int fi, ti;
        faceTorsoForEmotion(items[best].emotion, items[best].intensity, fi, ti);
        items[best].priority = 0;
        if (fi >= 0 && foundF < 0) foundF = fi;
        if (ti >= 0 && foundT < 0) foundT = ti;
    }
    if (foundF < 0) foundF = neutralFaceIndex();
    if (foundT < 0) foundT = neutralTorsoIndex();
    return composeFromIndices(-1, foundF, foundT, maskInsideIsHigh);
}
```

Ensure `#include <vector>` is available (it is, via headers) and that
`comic_semantics.h`/`comic_angles.h` are included.

- [ ] **Step 5: Run to verify it passes**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_emotion_pose.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp libcomic/comic_semantics.cpp -o build/test_emotion_pose && ./build/test_emotion_pose ../v1.0-pre-modern/comicart/avatars connor mike
```
Expected: PASS — prints `emotion_pose OK: simple shout WxH, complex happy WxH`.

- [ ] **Step 6: Regression — neutral compose unchanged**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_body.cpp libcomic/comic_avatar.cpp libcomic/comic_dib.cpp libcomic/comic_compose.cpp libcomic/png_writer.cpp -lz -o build/test_body
./build/test_body ../v1.0-pre-modern/comicart/avatars connor /tmp/e_connor.png
./build/test_body ../v1.0-pre-modern/comicart/avatars mike /tmp/e_mike.png
```
Expected: PASS — connor composed 166x421, mike composed 209x440 (unchanged from before the refactor).

- [ ] **Step 7: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_avatar.h v1.0-pre-mac/libcomic/comic_avatar.cpp v1.0-pre-mac/libcomic/test_emotion_pose.cpp
git commit -m "Select pose from text emotion; composeBodyForText"
```

---

## Task 4: Wire into Makefile + apps + verify visually

**Files:**
- Modify: `v1.0-pre-mac/Makefile`
- Modify: `v1.0-pre-mac/mac/main.mm`
- Modify: `v1.0-pre-mac/mac/render_panel.mm`

- [ ] **Step 1: Add comic_semantics.cpp to CORE_SRC**

In `Makefile`, change:
```make
CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp libcomic/comic_compose.cpp
```
to:
```make
CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp libcomic/comic_compose.cpp libcomic/comic_semantics.cpp
```

- [ ] **Step 2: main.mm — drive expression from the text**

The current `-loadCharacter:` composes the neutral body and stores it; text is
applied separately in `-sayChanged:`. Change the model so the displayed pose
reflects the current text. In `main.mm`:

Add an ivar to hold the current avatar name (so text changes can re-compose):
find the `@implementation AppController {` ivar block and add:
```cpp
    NSString* _currentName;
```

Replace `-loadCharacter:` to remember the name and delegate to a new recompose:
```cpp
- (void)loadCharacter:(NSString*)name {
    _currentName = [name copy];
    [self recompose];
}

- (void)recompose {
    // On any failure, clear the current figure.
    auto fail = [&](NSString* why) {
        NSLog(@"%@ %@", why, _currentName);
        if (_comic.bodyImage) { CGImageRelease(_comic.bodyImage); _comic.bodyImage = NULL; }
        _comic.bodyW = _comic.bodyH = 0;
        [_comic setNeedsDisplay:YES];
    };
    if (!_currentName) { fail(@"no character"); return; }
    auto av = comic::Avatar::load(_avatarDir, std::string(_currentName.UTF8String));
    if (!av) { fail(@"could not load"); return; }
    std::string text = _comic.text ? std::string(_comic.text.UTF8String) : std::string();
    comic::ComposedBody body = av->composeBodyForText(text, /*maskInsideIsHigh=*/true);
    if (!body.valid()) { fail(@"could not compose"); return; }
    if (_comic.bodyImage) CGImageRelease(_comic.bodyImage);
    _comic.bodyImage = [self makeImageFromRGBA:body.rgba width:body.width height:body.height];
    _comic.bodyW = body.width;
    _comic.bodyH = body.height;
    [_comic setNeedsDisplay:YES];
}
```

Update `-sayChanged:` to re-compose (so a new line changes the expression):
```cpp
- (void)sayChanged:(id)sender {
    _comic.text = [_say stringValue];
    [self recompose];
}
```

`-pickerChanged:` already calls `loadCharacter:` — leave it. Note the initial
`_comic.text` is set before `loadCharacter:` in `applicationDidFinishLaunching`;
that ordering already works (text set, then loadCharacter→recompose reads it).
If `loadCharacter:` is currently called before `_comic.text` is set, move the
`_comic.text = @"...";` line ABOVE the `[self loadCharacter:...]` call.

- [ ] **Step 3: render_panel.mm — use composeBodyForText(text)**

Replace the compose call:
```cpp
        comic::ComposedBody cb = av->composeNeutralBody(/*maskInsideIsHigh=*/true);
```
with:
```cpp
        comic::ComposedBody cb = av->composeBodyForText(text, /*maskInsideIsHigh=*/true);
```
(`text` is already the third CLI arg in render_panel.mm.)

- [ ] **Step 4: Build everything**

Run:
```bash
cd v1.0-pre-mac && make clean && make app render_panel test_art
```
Expected: all link, no errors/warnings.

- [ ] **Step 5: Render expression variants (visual verification)**

Run:
```bash
cd v1.0-pre-mac
./build/render_panel ../v1.0-pre-modern/comicart/avatars connor "HELLO EVERYONE!!!" /tmp/expr_shout.png
./build/render_panel ../v1.0-pre-modern/comicart/avatars connor "that is so funny :)" /tmp/expr_happy.png
./build/render_panel ../v1.0-pre-modern/comicart/avatars connor "oh no :(" /tmp/expr_sad.png
./build/render_panel ../v1.0-pre-modern/comicart/avatars connor "just a plain line" /tmp/expr_neutral.png
./build/render_panel ../v1.0-pre-modern/comicart/avatars mike "Hello there friends" /tmp/expr_wave.png
```
Expected: five PNGs written, exit 0 each. The controller/human reviewer should
open them and confirm the shout/happy/sad poses differ from neutral (exact
appearance depends on which poses each character actually has; connor is a good
test because it has 15 bodies across the emotion wheel).

- [ ] **Step 6: Commit**

```bash
git add v1.0-pre-mac/Makefile v1.0-pre-mac/mac/main.mm v1.0-pre-mac/mac/render_panel.mm
git commit -m "Drive character expression from typed text in app + render_panel"
```

---

## Task 5: Update docs

**Files:**
- Modify: `v1.0-pre-mac/README.md`

- [ ] **Step 1: Move text→emotion out of "deferred" into "What works"**

In `README.md`: remove the "Text → emotion/gesture analysis: the app always uses
the neutral pose" bullet from the deferred list. Add a "What works" bullet:
"Typed text drives the character's expression/pose via the ported rule engine
(e.g. `!!!`/ALL CAPS → shout, `:)` → happy, `:(` → sad, `Hi`/`Hello` → wave,
`lol` → laugh); no match falls back to neutral." Add a one-line note that the
ANGRY/SCARED/BORED emotions have no text triggers in the original rule set (so
they're only reachable via a pose that matches another emotion's angle).

- [ ] **Step 2: Commit**

```bash
git add v1.0-pre-mac/README.md
git commit -m "Document text-driven expressions in the macOS MVP"
```

---

## Notes for the implementer

- **Faithfulness over cleverness:** reproduce the original rules exactly (same
  strings, same strengths). Do NOT add new triggers, sentiment analysis, or
  emoji beyond what the table lists. ANGRY/SCARED/BORED intentionally have no
  triggers.
- **The `emotion <= 2*PI` guard is load-bearing:** wheel emotions use angular
  nearest-match; gesture sentinels (WAVE etc., value > 2π) use exact-match, so a
  gesture only appears if the avatar actually has a pose tagged with that exact
  sentinel. This is why waving may or may not visibly change a given character.
- **PI precision:** `comic_angles.h` uses the original's `3.14159`. The
  emotion-wheel angles in comic_semantics/comic_avatar use full-precision PI
  (as the existing `emotionToFloat` table already does). This matches the
  original, which computed wheel angles with full PI but normalized with 3.14159
  — keep both as-is; the nearest-match is robust to the tiny difference.
- **No premultiplication / coordinate changes:** this task is pose *selection*;
  compositing and rendering are unchanged from the AT_COMPLEX work.
- **If `composeBodyForText` ever returns empty** for a valid avatar+text, it
  should have fallen back to neutral — check the fallback path first.
