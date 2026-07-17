# Multi-Panel Comic Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the macOS Comic Chat port (`v1.0-pre-mac/`) from a single replaceable panel into an accumulating, scrollable **comic page** — each entered line appends a panel to a grid, and the strip grows as the conversation proceeds.

**Architecture:** Add a pure `PageLayout` (grid geometry: panel index → cell rect) to `libcomic`, headless-testable. Add a headless `render_page` tool that composes one panel per line and tiles them via `PageLayout` into one PNG — this de-risks the per-cell coordinate transform before any AppKit work. Then make the app retain a history of composed panels and render them in an `NSScrollView`-hosted view that grows downward, auto-scrolling to the newest. The existing per-panel `Panel`/`CoreGraphicsRenderer` are reused unchanged per cell.

**Tech Stack:** C++17 (`libcomic`, namespace `comic`, no OS deps), Objective-C++/Cocoa (`mac/`), `clang++`, hand-written `Makefile`. Tests are standalone `main()`+`assert` programs compiled ad-hoc (matching the existing `test_*.cpp`), plus headless PNG renders for visual verification.

---

## Background: the original grid

From `v1.0-pre-modern/panel.cpp` (`CUnitPanelPage`): panels tile in a grid of
`panelsPerRow` columns; each cell is `unitWidth × unitHeight` separated by
`hInterstice`/`vInterstice` (defaults 144 TWIPs). `RefreshPanelN` (panel.cpp:206)
computes a panel's rect from `(index, panelsPerRow, unitWidth, unitHeight,
interstice)`. Panel-advance (`AddLine`, panel.cpp:381) starts a new panel when
the same avatar already spoke in the current one — and in our single-user
comic-only app **every line is the same speaker, so this collapses to one new
panel per committed line.** We therefore need no multi-body layout.

## Current state (what we build on)

- `libcomic/comic_panel.{h,cpp}` — `Panel(int w, int h, FontHandle)` with
  `setBody`/`setText`/`draw(IComicRenderer&)`; draws a framed white panel + one
  balloon + one centered body, all from its own (0,0). **Unchanged by this plan.**
- `mac/CoreGraphicsRenderer.{h,mm}` — `CoreGraphicsRenderer(CGContextRef ctx,
  int heightPts)`; flips Y with `heightPts`. **Unchanged**; we position each
  panel by translating the CTM and constructing one renderer per cell.
- `mac/main.mm` — a single non-scrolling `ComicView` whose `drawRect:` builds one
  full-view `Panel`. `recompose` composes the current character+text and stores
  one `CGImage`, discarding the previous. `sayChanged:` fires on Return.
- `mac/render_panel.mm` — headless single-panel render to PNG (the model for the
  new `render_page` tool).

## File structure

- Create `v1.0-pre-mac/libcomic/comic_page.h` / `.cpp` — `PageLayout` grid math.
- Create `v1.0-pre-mac/libcomic/test_page.cpp` — grid-geometry assertions.
- Create `v1.0-pre-mac/mac/render_page.mm` — headless multi-panel PNG tool.
- Modify `v1.0-pre-mac/mac/main.mm` — panel history + scrollable page view.
- Modify `v1.0-pre-mac/Makefile` — add `comic_page.cpp` to `CORE_SRC`; add a
  `render_page` target.
- Modify `v1.0-pre-mac/README.md` — document the comic page.

---

## Task 1: PageLayout grid geometry (pure, testable)

**Files:**
- Create: `v1.0-pre-mac/libcomic/comic_page.h`
- Create: `v1.0-pre-mac/libcomic/comic_page.cpp`
- Create: `v1.0-pre-mac/libcomic/test_page.cpp`

- [ ] **Step 1: Write the header**

```cpp
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
```

- [ ] **Step 2: Write the failing test** (`test_page.cpp`)

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include <cassert>
#include <cstdio>
#include "comic_page.h"
using namespace comic;
int main() {
    // 200x300 cells, 2 cols, 10px gaps.
    PageLayout p(200, 300, 2, 10, 10);
    assert(p.cols() == 2);
    assert(p.pageWidth() == 200 * 2 + 10);         // 410

    // rows
    assert(p.rowsFor(0) == 0);
    assert(p.rowsFor(1) == 1);
    assert(p.rowsFor(2) == 1);
    assert(p.rowsFor(3) == 2);
    assert(p.rowsFor(4) == 2);

    // page height: 1 row = 300; 2 rows = 300+10+300 = 610.
    assert(p.pageHeight(0) == 0);
    assert(p.pageHeight(1) == 300);
    assert(p.pageHeight(2) == 300);
    assert(p.pageHeight(3) == 610);

    // cell 0: top-left.
    Rect r0 = p.cellRect(0);
    assert(r0.left == 0 && r0.top == 0 && r0.right == 200 && r0.bottom == 300);
    // cell 1: second column.
    Rect r1 = p.cellRect(1);
    assert(r1.left == 210 && r1.top == 0 && r1.right == 410 && r1.bottom == 300);
    // cell 2: second row, first column.
    Rect r2 = p.cellRect(2);
    assert(r2.left == 0 && r2.top == 310 && r2.right == 200 && r2.bottom == 610);

    std::printf("test_page OK\n");
    return 0;
}
```

- [ ] **Step 3: Run to verify it fails**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_page.cpp libcomic/comic_page.cpp -o build/test_page
```
Expected: FAIL — link error, `PageLayout` members undefined.

- [ ] **Step 4: Implement `comic_page.cpp`**

```cpp
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
```

- [ ] **Step 5: Run to verify it passes**

Run:
```bash
cd v1.0-pre-mac
clang++ -std=c++17 -Ilibcomic libcomic/test_page.cpp libcomic/comic_page.cpp -o build/test_page && ./build/test_page
```
Expected: PASS — prints `test_page OK`.

- [ ] **Step 6: Commit**

```bash
git add v1.0-pre-mac/libcomic/comic_page.h v1.0-pre-mac/libcomic/comic_page.cpp v1.0-pre-mac/libcomic/test_page.cpp
git commit -m "Add PageLayout grid geometry for multi-panel pages"
```

---

## Task 2: Headless render_page tool (de-risks the per-cell transform)

Compose one panel per input line and tile them into a single PNG via
`PageLayout`. This proves the per-cell CTM translate + renderer works BEFORE any
scroll-view code — and is visually verifiable.

**Files:**
- Create: `v1.0-pre-mac/mac/render_page.mm`
- Modify: `v1.0-pre-mac/Makefile` (add `comic_page.cpp` to CORE_SRC; add target)

- [ ] **Step 1: Add comic_page.cpp to CORE_SRC**

In `Makefile`, change:
```make
CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp libcomic/comic_compose.cpp libcomic/comic_semantics.cpp
```
to add `libcomic/comic_page.cpp`:
```make
CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp libcomic/comic_compose.cpp libcomic/comic_semantics.cpp libcomic/comic_page.cpp
```

- [ ] **Step 2: Add the render_page target to the Makefile**

After the existing `render_panel` target block, add:
```make
$(BUILD)/render_page: $(CORE_OBJ) $(PANEL_OBJ) $(BUILD)/CoreGraphicsRenderer.o $(BUILD)/render_page.o
	$(CXX) $(CXXFLAGS) $^ -framework CoreGraphics -framework CoreText -framework ImageIO -framework Foundation -framework UniformTypeIdentifiers -o $@

.PHONY: render_page
render_page: $(BUILD)/render_page
```
(This mirrors the existing `render_panel` target exactly, swapping the source
name. `PANEL_OBJ` is `$(BUILD)/comic_panel.o`; confirm by reading the Makefile
and match whatever the render_panel target uses.)

- [ ] **Step 3: Write render_page.mm**

```objc
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// render_page — headless render of a MULTI-panel comic page: one panel per
// input line, composed via composeBodyForText and tiled via PageLayout into one
// PNG. Proves the per-cell coordinate transform used by the scrollable app view.
//
// Usage: render_page <avatarDir> <name> <cols> <out.png> "line1" "line2" ...

#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "CoreGraphicsRenderer.h"
#include "comic_avatar.h"
#include "comic_compose.h"
#include "comic_page.h"
#include "comic_panel.h"

static CGImageRef MakeImage(const std::vector<comic::u8>& rgba, int w, int h) {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmp = CGBitmapContextCreate((void*)rgba.data(), w, h, 8, w * 4, cs,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef img = CGBitmapContextCreateImage(bmp);
    CGContextRelease(bmp);
    CGColorSpaceRelease(cs);
    return img;
}

static bool SavePng(CGImageRef img, NSString* path) {
    CFURLRef url = (__bridge CFURLRef)[NSURL fileURLWithPath:path];
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
    if (!dst) return false;
    CGImageDestinationAddImage(dst, img, nullptr);
    bool ok = CGImageDestinationFinalize(dst);
    CFRelease(dst);
    return ok;
}

int main(int argc, const char* argv[]) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s <avatarDir> <name> <cols> <out.png> \"line\" ...\n", argv[0]);
        return 2;
    }
    @autoreleasepool {
        std::string dir = argv[1], name = argv[2];
        int cols = atoi(argv[3]);
        if (cols < 1) cols = 1;
        NSString* out = [NSString stringWithUTF8String:argv[4]];
        std::vector<std::string> lines;
        for (int i = 5; i < argc; ++i) lines.push_back(argv[i]);

        auto av = comic::Avatar::load(dir, name);
        if (!av) { fprintf(stderr, "FAIL load %s\n", name.c_str()); return 1; }

        CTFontRef font = CTFontCreateWithName(CFSTR("Comic Sans MS"), 18.0, nullptr);
        if (!font) font = CTFontCreateWithName(CFSTR("Helvetica"), 18.0, nullptr);

        const int panelW = 300, panelH = 400, gap = 12;
        comic::PageLayout layout(panelW, panelH, cols, gap, gap);
        int n = (int)lines.size();
        int W = layout.pageWidth();
        int H = layout.pageHeight(n);
        if (H <= 0) { fprintf(stderr, "FAIL no lines\n"); return 1; }

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(nullptr, W, H, 8, W * 4, cs,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(cs);

        // Gray page background.
        CGContextSetRGBFillColor(ctx, 0.85, 0.85, 0.85, 1.0);
        CGContextFillRect(ctx, CGRectMake(0, 0, W, H));

        for (int i = 0; i < n; ++i) {
            comic::ComposedBody cb = av->composeBodyForText(lines[i], /*maskInsideIsHigh=*/true);
            CGImageRef body = cb.valid() ? MakeImage(cb.rgba, cb.width, cb.height) : nullptr;

            // Top-down cell rect -> bottom-left origin in the y-up bitmap.
            comic::Rect cell = layout.cellRect(i);
            CGFloat cellBottomUpY = H - cell.bottom; // flip the cell's top-down y
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx, cell.left, cellBottomUpY);

            comic::CoreGraphicsRenderer renderer(ctx, panelH);
            comic::Panel panel(panelW, panelH, (const void*)font);
            if (body) {
                comic::PanelBody pb; pb.image = (const void*)body; pb.width = cb.width; pb.height = cb.height;
                panel.setBody(pb);
            }
            panel.setText(lines[i]);
            panel.draw(renderer);

            CGContextRestoreGState(ctx);
            if (body) CGImageRelease(body);
        }

        CGImageRef result = CGBitmapContextCreateImage(ctx);
        bool ok = SavePng(result, out);
        CGImageRelease(result);
        CGContextRelease(ctx);
        fprintf(ok ? stdout : stderr, "%s %s (%d panels, %dx%d)\n",
                ok ? "wrote" : "FAIL write", argv[4], n, W, H);
        return ok ? 0 : 1;
    }
}
```

Key transform note for the implementer: the panel draws top-down from its own
(0,0); `CoreGraphicsRenderer(ctx, panelH)` flips within a single panel height.
Translating the CTM to the cell's bottom-left corner in the y-up bitmap
(`H - cell.bottom`) positions that panel in its grid cell. This is the exact
transform the app view will reuse.

- [ ] **Step 4: Build**

Run:
```bash
cd v1.0-pre-mac && make render_page
```
Expected: links clean (comic_page.o in CORE_OBJ; render_page.o compiled).

- [ ] **Step 5: Render a multi-panel page and verify visually**

Run:
```bash
cd v1.0-pre-mac
./build/render_page ../v1.0-pre-modern/comicart/avatars connor 2 build/page2.png \
  "Hello there!" "HELLO!!!" "that is funny :)" "oh no :("
```
Expected: writes `build/page2.png` — a 2-column grid of 4 panels. The
controller/human MUST open it and confirm: 4 distinct panels laid out 2×2, each
framed, each with its balloon text and connor in the matching pose (wave/shout/
happy/sad), no overlap, no clipping, panels in reading order (0 top-left, 1
top-right, 2 bottom-left, 3 bottom-right).

- [ ] **Step 6: Commit**

```bash
git add v1.0-pre-mac/Makefile v1.0-pre-mac/mac/render_page.mm
git commit -m "Add headless render_page tool tiling panels via PageLayout"
```

---

## Task 3: Scrollable multi-panel app view + history

Retain a composed panel per entered line and render them all in a scrollable
view that grows downward, auto-scrolling to the newest.

**Files:**
- Modify: `v1.0-pre-mac/mac/main.mm`

- [ ] **Step 1: Add a PanelRecord history + make ComicView render the page**

Replace the current single-panel `ComicView` with a page view. In `main.mm`,
define a small record and change `ComicView` to hold an ordered list:

Add near the top (after includes), an Obj-C++-friendly record. Since `ComicView`
needs C++ members, keep them as instance variables in the `@implementation`
block (the file already mixes C++ and Obj-C).

Replace the `ComicView` interface/implementation with:

```objc
// A committed panel: its composed body image (owned) + text.
struct PanelRecord {
    CGImageRef image;   // retained; released in ComicView dealloc / clearPanels
    int bodyW;
    int bodyH;
    std::string text;
};

@interface ComicView : NSView
@property(nonatomic, assign) CTFontRef font;
- (void)addPanelWithImage:(CGImageRef)img width:(int)w height:(int)h text:(const std::string&)text;
- (void)clearPanels;
- (int)panelCount;
@end

@implementation ComicView {
    std::vector<PanelRecord> _panels;
}

- (BOOL)isFlipped { return NO; }

// Cell size + columns for the page. Fixed for now; column-fit can come later.
static const int kPanelW = 300;
static const int kPanelH = 400;
static const int kGap = 12;

- (comic::PageLayout)layout {
    return comic::PageLayout(kPanelW, kPanelH, [self columns], kGap, kGap);
}

- (int)columns {
    int avail = (int)self.bounds.size.width;
    int c = (avail + kGap) / (kPanelW + kGap);
    return c < 1 ? 1 : c;
}

- (void)addPanelWithImage:(CGImageRef)img width:(int)w height:(int)h text:(const std::string&)text {
    if (img) CGImageRetain(img);
    _panels.push_back(PanelRecord{img, w, h, text});
    [self relayout];
}

- (void)clearPanels {
    for (auto& r : _panels) if (r.image) CGImageRelease(r.image);
    _panels.clear();
    [self relayout];
}

- (int)panelCount { return (int)_panels.size(); }

// Resize the (document) view to fit all panels, then redraw + scroll to newest.
- (void)relayout {
    comic::PageLayout lay = [self layout];
    int h = lay.pageHeight((int)_panels.size());
    if (h < (int)self.enclosingScrollView.contentSize.height)
        h = (int)self.enclosingScrollView.contentSize.height;
    NSRect f = self.frame;
    f.size.height = h;
    // Keep width matched to the scroll view's content width.
    f.size.width = self.enclosingScrollView.contentSize.width;
    self.frame = f;
    [self setNeedsDisplay:YES];
    // Auto-scroll so the newest (bottom) panel is visible. Non-flipped view:
    // the newest panel is at the BOTTOM in reading order but the TOP in y-up
    // document coords — verify scroll direction visually and adjust.
    [self scrollPoint:NSMakePoint(0, 0)];
}

- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSetRGBFillColor(ctx, 0.85, 0.85, 0.85, 1.0);
    CGContextFillRect(ctx, NSRectToCGRect(self.bounds));

    comic::PageLayout lay = [self layout];
    int H = (int)self.bounds.size.height;
    for (int i = 0; i < (int)_panels.size(); ++i) {
        const PanelRecord& rec = _panels[i];
        comic::Rect cell = lay.cellRect(i);
        CGFloat cellBottomUpY = H - cell.bottom;
        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, cell.left, cellBottomUpY);
        comic::CoreGraphicsRenderer renderer(ctx, kPanelH);
        comic::Panel panel(kPanelW, kPanelH, (const void*)self.font);
        if (rec.image) {
            comic::PanelBody body; body.image = (const void*)rec.image;
            body.width = rec.bodyW; body.height = rec.bodyH;
            panel.setBody(body);
        }
        panel.setText(rec.text);
        panel.draw(renderer);
        CGContextRestoreGState(ctx);
    }
}

- (void)dealloc {
    for (auto& r : _panels) if (r.image) CGImageRelease(r.image);
}
@end
```

Add `#include "comic_page.h"` to the includes at the top of `main.mm`.

IMPORTANT coordinate note: the reading order is index 0 at the TOP. In a
non-flipped `NSView`, y-up document coords put index 0 at the top when we compute
`cellBottomUpY = viewHeight - cell.bottom` and the view height equals the page
height. Because the panel count (and thus page/view height) is known at draw
time via `self.bounds`, this stays consistent. The `scrollPoint:` direction to
reveal the newest panel MUST be verified on the running app and adjusted (it may
need `NSMakePoint(0, NSMaxY(self.bounds))` or `0` depending on flipped-ness) —
this is the one thing to confirm live in Step 4.

- [ ] **Step 2: Update AppController to commit panels + host a scroll view**

In `AppController`'s ivars, keep `_comic` (now the document view) and add the
scroll view. Change `applicationDidFinishLaunching:` to embed `_comic` in an
`NSScrollView`, and change `sayChanged:` to COMMIT a panel (compose the current
character+text, append to `_comic`) instead of recomposing in place.

Replace the relevant parts:

Ivars — add:
```objc
    NSScrollView* _scroll;
```

`-recompose` is replaced by `-commitPanel` (compose + append):
```objc
- (void)commitPanel {
    if (!_currentName) return;
    NSString* line = [_say stringValue];
    if ([line length] == 0) return;               // ignore empty submits
    auto av = comic::Avatar::load(_avatarDir, std::string(_currentName.UTF8String));
    if (!av) { NSLog(@"could not load %@", _currentName); return; }
    std::string text = std::string(line.UTF8String);
    comic::ComposedBody body = av->composeBodyForText(text, /*maskInsideIsHigh=*/true);
    if (!body.valid()) { NSLog(@"could not compose %@", _currentName); return; }
    CGImageRef img = [self makeImageFromRGBA:body.rgba width:body.width height:body.height];
    [_comic addPanelWithImage:img width:body.width height:body.height text:text];
    if (img) CGImageRelease(img);                  // addPanel retains its own ref
    [_say setStringValue:@""];                     // clear the say box for the next line
}
```

`-loadCharacter:` just remembers the current speaker (no recompose):
```objc
- (void)loadCharacter:(NSString*)name { _currentName = [name copy]; }
```

`-pickerChanged:` stays (calls loadCharacter). `-sayChanged:` commits:
```objc
- (void)sayChanged:(id)sender { [self commitPanel]; }
```

In `applicationDidFinishLaunching:`, replace the direct `_comic` subview with a
scroll-view-hosted document view:
```objc
    _comic = [[ComicView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 48)];
    _comic.font = _font;

    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 48)];
    [_scroll setHasVerticalScroller:YES];
    [_scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_scroll setDocumentView:_comic];
    [content addSubview:_scroll];

    [self loadCharacter:[_picker titleOfSelectedItem]];
```
(Remove the old `[content addSubview:_comic]` and the initial `_comic.text = ...`
line; the page starts empty and fills as you type.)

- [ ] **Step 3: Build**

Run:
```bash
cd v1.0-pre-mac && make app
```
Expected: links clean, no errors/warnings.

- [ ] **Step 4: Run and verify live (controller/human)**

Run:
```bash
cd v1.0-pre-mac && make run
```
In the window: pick a character, type a line + Return → a panel appears. Type
another line + Return → a second panel appears beside/below the first. Confirm:
panels accumulate in a grid (don't replace each other), the view scrolls when
they exceed the window, the newest panel is revealed, and changing the character
affects the NEXT committed panel. If the auto-scroll reveals the wrong end,
adjust the `scrollPoint:` in `relayout` and rebuild.

(Because live-app screen capture may be unavailable in headless sessions, the
headless `render_page` from Task 2 is the authoritative visual check for panel
layout; Step 4 verifies the interactive commit/scroll behavior.)

- [ ] **Step 5: Commit**

```bash
git add v1.0-pre-mac/mac/main.mm
git commit -m "Accumulate committed panels in a scrollable comic page"
```

---

## Task 4: Update docs

**Files:**
- Modify: `v1.0-pre-mac/README.md`

- [ ] **Step 1: Document the comic page**

In `README.md`: update the intro/"What works" to say the app now builds a
**scrollable comic page** — each entered line appends a panel to a grid (one
panel per line, the single-speaker case of the original's panel-advance rule),
rather than replacing a single panel. Remove "Multi-panel pages" from the
deferred list (keep history/save-load/print deferred). Mention the `render_page`
headless tool alongside `render_panel` in the build/verify section:
```sh
make render_page   # headless: render a multi-panel page to PNG
./build/render_page ../v1.0-pre-modern/comicart/avatars connor 2 page.png "Hi!" "HELLO!!!" ":)"
```

- [ ] **Step 2: Commit**

```bash
git add v1.0-pre-mac/README.md
git commit -m "Document the scrollable comic page in the macOS MVP"
```

---

## Notes for the implementer

- **The per-cell transform is the one real risk.** Task 2 (`render_page`) exists
  to nail it headlessly and PNG-verifiably before the scroll view. If Task 2's
  PNG looks right, Task 3 reuses the identical `translate + CoreGraphicsRenderer(ctx,
  panelH)` per cell — so if the app view misbehaves, it's a scroll-view / view-
  height / flipped-ness issue, not the panel transform.
- **Panel-advance policy = one panel per committed line.** This is the faithful
  single-speaker collapse of `AddLine`. Do NOT implement the multi-speaker
  "extend the current panel" branch or multi-body layout — out of scope.
- **`comic_panel.cpp` and `CoreGraphicsRenderer` are unchanged.** If you find
  yourself editing them, stop and reconsider — the increment is a page/history
  layer around the existing per-panel renderer.
- **Memory:** `PanelRecord` owns a retained `CGImageRef`; release on clear and in
  `dealloc`. `addPanelWithImage:` retains its own ref, so the caller releases the
  image it passed (as shown in `commitPanel`).
- **Column count** is derived from view width (`columns`); a fixed default is
  fine. Full `FitPanelsWide` reflow-on-resize can be a later refinement — but the
  grid must at least not overlap at the default window size.
- **Empty/failed compose:** a line that fails to compose should be skipped
  (logged), not appended as a blank panel.
