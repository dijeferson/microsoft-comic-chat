// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Ported from textpose.cpp: the fixed rule table + GetEmotionsFromString.

#include "comic_semantics.h"

#include <cctype>

namespace comic {

namespace {

const double kPI = 3.14159265358979323846;
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

    if (checkForUppers(text)) o.add(EM_SHOUT, 1.0f, 9);

    if (text.find("!!!") != std::string::npos) o.add(EM_SHOUT, 1.0f, 9);
    if (text.find(":)") != std::string::npos) o.add(EM_HAPPY, 1.0f, 10);
    if (text.find(":-)") != std::string::npos) o.add(EM_HAPPY, 1.0f, 10);
    if (text.find(":(") != std::string::npos) o.add(EM_SAD, 1.0f, 10);
    if (text.find(":-(") != std::string::npos) o.add(EM_SAD, 1.0f, 10);
    if (text.find(";-)") != std::string::npos) o.add(EM_COY, 1.0f, 10);
    if (text.find(";)") != std::string::npos) o.add(EM_COY, 1.0f, 10);
    if (lower.find("hehe") != std::string::npos) o.add(EM_LAUGH, 1.0f, 11);

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

    {
        const std::string term = ".!?";
        size_t i = 0;
        const std::string& s = lower;
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
