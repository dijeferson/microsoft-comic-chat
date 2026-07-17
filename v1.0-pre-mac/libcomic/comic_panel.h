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

class Panel {
public:
    Panel(int widthPts, int heightPts, FontHandle font)
        : width_(widthPts), height_(heightPts), font_(font) {}

    void setBody(const PanelBody& body) { body_ = body; }
    void setText(const std::string& text) { text_ = text; }

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
    std::string text_;

    // Break text_ into lines that each fit within maxWidth points.
    std::vector<std::string> wrap(IComicRenderer& r, int maxWidth);
};

} // namespace comic

#endif // COMIC_PANEL_H
