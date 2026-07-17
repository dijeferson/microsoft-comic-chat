// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "comic_panel.h"

#include <algorithm>
#include <sstream>

namespace comic {

namespace {
constexpr int kMargin = 12;        // panel inner margin (points)
constexpr int kBalloonPad = 10;    // text inset inside balloon
constexpr RGBA kBlack{0, 0, 0, 255};
constexpr RGBA kWhite{255, 255, 255, 255};
} // namespace

// Greedy word wrap using the renderer's text measurement. Mirrors the intent of
// balloon.cpp's BreakIntoLines (keep whole words; only break when a word won't
// fit on the current line), which is why measureText is on the seam.
std::vector<std::string> Panel::wrap(IComicRenderer& r, int maxWidth) {
    std::vector<std::string> lines;
    std::istringstream iss(text_);
    std::string word, line;
    while (iss >> word) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (r.measureText(candidate.c_str(), font_).cx <= maxWidth || line.empty()) {
            line = candidate;
        } else {
            lines.push_back(line);
            line = word;
        }
    }
    if (!line.empty()) lines.push_back(line);
    if (lines.empty()) lines.push_back("");
    return lines;
}

void Panel::draw(IComicRenderer& r) {
    // Panel background + frame.
    r.beginPath();
    r.moveTo({0, 0});
    r.lineTo({width_, 0});
    r.lineTo({width_, height_});
    r.lineTo({0, height_});
    r.closeSubpath();
    r.fillAndStrokePath(kWhite, StrokeStyle{2.0, kBlack});

    // --- Backdrop: fill the panel interior behind everything ----------------
    // Ports CBackDrop::Draw, which blits the backdrop DIB scaled to the panel
    // rect. Here the dest is the panel interior (inside the 2px frame). If no
    // backdrop is set, this is skipped and the panel stays plain white — the
    // exact pre-backdrop behavior. NOTE: for maskless character bodies whose
    // transparency is white-keyed (see comic_compose stampPart), a non-white
    // backdrop will show the body's white background as a box; that's the known
    // mask/white-key limitation and is out of scope here.
    if (backdrop_.image && backdrop_.width > 0 && backdrop_.height > 0) {
        Rect dest{1, 1, width_ - 1, height_ - 1};
        r.drawImage(backdrop_.image, dest);
    }

    // --- Body: centered, sitting on the bottom margin -----------------------
    int bodyBottom = height_ - kMargin;
    int bodyX = width_ / 2;
    Point tailTarget{width_ / 2, height_ / 2};
    if (body_.image && body_.width > 0 && body_.height > 0) {
        // Scale the body to fit roughly the lower 60% of the panel height.
        int maxBodyH = static_cast<int>(height_ * 0.6);
        double scale = std::min(1.0, static_cast<double>(maxBodyH) / body_.height);
        int bw = static_cast<int>(body_.width * scale);
        int bh = static_cast<int>(body_.height * scale);
        Rect dest{bodyX - bw / 2, bodyBottom - bh, bodyX + bw / 2, bodyBottom};
        r.drawImage(body_.image, dest);
        // Tail points at the top-center of the figure (near the head).
        tailTarget = Point{bodyX, dest.top + bh / 10};
    }

    // --- Balloon: top of panel, wrapping the text ---------------------------
    if (!text_.empty()) {
        int lineHeight = 0, ascent = 0;
        r.fontMetrics(font_, lineHeight, ascent);
        if (lineHeight <= 0) lineHeight = 16;

        int maxTextWidth = width_ - 2 * kMargin - 2 * kBalloonPad;
        std::vector<std::string> lines = wrap(r, maxTextWidth);

        int textW = 0;
        for (const auto& ln : lines)
            textW = std::max(textW, r.measureText(ln.c_str(), font_).cx);
        int textH = static_cast<int>(lines.size()) * lineHeight;

        int balloonW = textW + 2 * kBalloonPad;
        int balloonH = textH + 2 * kBalloonPad;
        int bx = (width_ - balloonW) / 2;
        int by = kMargin;
        int bxr = bx + balloonW;
        int byb = by + balloonH;

        // Ornate balloon outline selected by speech mode (comic_balloon):
        //   Say -> wavy Woodring spline, Think -> cloud + bubble trail,
        //   Whisper -> thin light outline, Shout -> spiky burst.
        Rect box{bx, by, bxr, byb};
        drawBalloon(r, mode_, box, tailTarget);

        // Text lines.
        int tx = bx + kBalloonPad;
        int ty = by + kBalloonPad;
        for (const auto& ln : lines) {
            r.drawText(ln.c_str(), {tx, ty}, font_, kBlack);
            ty += lineHeight;
        }
    }
}

} // namespace comic
