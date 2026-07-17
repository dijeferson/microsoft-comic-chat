// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_page.h — grid geometry for a page of comic panels. Pure math (no OS,
// no drawing): maps a panel index to its top-down cell rect, mirroring the
// original CUnitPanelPage grid (panel.cpp). Row 0 (index 0) is the top-left,
// oldest panel; the page grows downward as panels are appended.

#ifndef COMIC_PAGE_H
#define COMIC_PAGE_H

#include "comic_types.h"

namespace comic {

class PageLayout {
public:
    // panelWidth/panelHeight: a single cell's size (points). cols: panels per
    // row (>= 1). hGap/vGap: spacing between cells (points).
    PageLayout(int panelWidth, int panelHeight, int cols, int hGap, int vGap);

    int cols() const { return cols_; }
    int panelWidth() const { return panelW_; }
    int panelHeight() const { return panelH_; }

    // Number of rows needed to hold `count` panels.
    int rowsFor(int count) const;

    // Total page size (points) needed to hold `count` panels, including gaps.
    int pageWidth() const;      // cols * panelW + (cols-1)*hGap
    int pageHeight(int count) const;

    // Top-down cell rect for panel `index` (index 0 = top-left). left/top are
    // the cell's top-left corner; right/bottom follow panelW/panelH.
    Rect cellRect(int index) const;

private:
    int panelW_, panelH_, cols_, hGap_, vGap_;
};

} // namespace comic

#endif // COMIC_PAGE_H
