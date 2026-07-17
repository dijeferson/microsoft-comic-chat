// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_page.h"
using namespace comic;
int main() {
    PageLayout p(200, 300, 2, 10, 10);
    assert(p.cols() == 2);
    assert(p.pageWidth() == 200 * 2 + 10);         // 410

    assert(p.rowsFor(0) == 0);
    assert(p.rowsFor(1) == 1);
    assert(p.rowsFor(2) == 1);
    assert(p.rowsFor(3) == 2);
    assert(p.rowsFor(4) == 2);

    assert(p.pageHeight(0) == 0);
    assert(p.pageHeight(1) == 300);
    assert(p.pageHeight(2) == 300);
    assert(p.pageHeight(3) == 610);                // 300+10+300

    Rect r0 = p.cellRect(0);
    assert(r0.left == 0 && r0.top == 0 && r0.right == 200 && r0.bottom == 300);
    Rect r1 = p.cellRect(1);
    assert(r1.left == 210 && r1.top == 0 && r1.right == 410 && r1.bottom == 300);
    Rect r2 = p.cellRect(2);
    assert(r2.left == 0 && r2.top == 310 && r2.right == 200 && r2.bottom == 610);

    std::printf("test_page OK\n");
    return 0;
}
