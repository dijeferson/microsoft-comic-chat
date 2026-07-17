// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_panel.h — minimal panel composition for the MVP: one avatar body + one
// speech balloon with word-wrapped text and a tail pointing at the speaker.
// A reduced stand-in for panel.cpp/balloon.cpp's layout; the full expert-system
// layout (multi-body placement, emotion posing, panel advance) comes later.

#ifndef COMIC_PANEL_H
#define COMIC_PANEL_H

#include <string>
#include <vector>

#include "comic_balloon.h"
#include "comic_renderer.h"
#include "comic_types.h"

namespace comic {

// The avatar bitmap already decoded to RGBA + its natural pixel size. The panel
// does not own the ImageHandle; the app builds/caches backend images.
struct PanelBody {
    ImageHandle image = nullptr;
    int width = 0;
    int height = 0;
};

// An optional backdrop image drawn as the panel background, behind the balloon
// and body. Like PanelBody, the panel does not own the handle. Ports
// CBackDrop/CBackDropArt: the backdrop fills the panel's drawing area, scaled.
struct PanelBackdrop {
    ImageHandle image = nullptr;
    int width = 0;
    int height = 0;
};

class Panel {
public:
    Panel(int widthPts, int heightPts, FontHandle font)
        : width_(widthPts), height_(heightPts), font_(font) {}

    void setBody(const PanelBody& body) { body_ = body; }
    void setText(const std::string& text) { text_ = text; }
    // Speech mode selects the balloon shape (default Say = wavy Woodring).
    void setSpeechMode(SpeechMode mode) { mode_ = mode; }
    SpeechMode speechMode() const { return mode_; }
    // Optional: draw a backdrop as the panel background. If not set (or a null
    // image), the panel renders exactly as before (plain white background).
    void setBackdrop(ImageHandle image, int width, int height) {
        backdrop_ = PanelBackdrop{image, width, height};
    }

    int width() const { return width_; }
    int height() const { return height_; }

    // Compute layout (wrapping text via the renderer's measureText) and draw the
    // panel: white background, framed border, balloon at top, body at bottom.
    void draw(IComicRenderer& r);

private:
    int width_;
    int height_;
    FontHandle font_;
    PanelBody body_;
    PanelBackdrop backdrop_;
    std::string text_;
    SpeechMode mode_ = SpeechMode::Say;

    // Break text_ into lines that each fit within maxWidth points.
    std::vector<std::string> wrap(IComicRenderer& r, int maxWidth);
};

} // namespace comic

#endif // COMIC_PANEL_H
