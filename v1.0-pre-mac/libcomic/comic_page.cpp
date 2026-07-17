// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "comic_page.h"

namespace comic {

PageLayout::PageLayout(int panelWidth, int panelHeight, int cols, int hGap, int vGap)
    : panelW_(panelWidth), panelH_(panelHeight),
      cols_(cols < 1 ? 1 : cols), hGap_(hGap), vGap_(vGap) {}

int PageLayout::rowsFor(int count) const {
    if (count <= 0) return 0;
    return (count + cols_ - 1) / cols_;
}

int PageLayout::pageWidth() const {
    return cols_ * panelW_ + (cols_ - 1) * hGap_;
}

int PageLayout::pageHeight(int count) const {
    int rows = rowsFor(count);
    if (rows == 0) return 0;
    return rows * panelH_ + (rows - 1) * vGap_;
}

Rect PageLayout::cellRect(int index) const {
    int row = index / cols_;
    int col = index % cols_;
    long left = static_cast<long>(col) * (panelW_ + hGap_);
    long top = static_cast<long>(row) * (panelH_ + vGap_);
    return Rect{left, top, left + panelW_, top + panelH_};
}

} // namespace comic
