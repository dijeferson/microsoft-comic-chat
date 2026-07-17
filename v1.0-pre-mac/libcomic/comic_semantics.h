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
