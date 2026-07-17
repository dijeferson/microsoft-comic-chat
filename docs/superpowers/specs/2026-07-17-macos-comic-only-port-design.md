# Design: Native macOS port of Comic Chat (comic-only mode)

**Date:** 2026-07-17
**Status:** Approved for planning
**Base source tree:** `v1.0-pre-modern/`

## Goal

Produce a **native macOS application** that runs Comic Chat's comic-generation
engine in **comic-only mode** — no IRC, no networking. The user types a line of
text, picks a character and emotion/gesture, and the app renders comic panels
using the original `.avb` character art. This is a genuine port of the existing
C++ engine (not a rewrite and not an emulation/compatibility layer).

Non-goals: IRC connectivity, multi-server support, OLE/COM automation, URL
detection, printing, and any Windows-only feature not needed to render a comic
offline.

## Why comic-only

The comic-generation "expert system" (text analysis → emotion/gesture →
character pose → panel/balloon layout) is the interesting, portable heart of
Comic Chat and is logically independent of networking. The code already has a
notion of "standalone" behavior gated on `CX_DISCONNECTED` (see
`panel.cpp:1096`). Cutting IRC removes the entire Windows sockets/TLS/protocol
surface and lets the port focus on the engine and rendering.

## Architecture

Two layers with a deliberate interface (the "seam") between them.

### Layer A — `libcomic` (portable C++ core, no OS dependencies)

The text-analysis and layout engine, extracted and de-Windowsed:

- **Pure-logic files** (~0 GDI references today): `semantic.cpp`,
  `textpose.cpp`, `avatar.cpp`, `bbox.cpp`, `spline.cpp`, `traj.cpp`,
  `vector2d.cpp`. Port work is mechanical: replace MFC containers
  (`CObList`/`CPtrList`/`POSITION`/`CString`) with `std::vector`/`std::string`,
  and `INT8/16/32`-style typedefs with `<cstdint>`.
- **Art loader**: `dib.cpp`, `avatario.cpp`. Already uses `FILE*`/`fread`.
  `CDIB` holds a `BITMAPINFO` + raw pixel bits in memory; keep that
  representation but strip the GDI `Draw()`/`CDC` methods.
- **Output of this layer:** a fully laid-out panel — positioned bitmaps
  (character bodies, backdrop) plus vector shapes (balloons, splines) and text
  runs, in the engine's TWIP coordinate space. No pixels are drawn here.

### The seam — abstract `IComicRenderer`

`panel.cpp` and `balloon.cpp` currently call `CDC` methods directly (`BitBlt`,
`SelectObject`+`Polygon`, `TextOut`, pen/brush). These ~5 draw entry points
(`CUnitPanel::Draw`, the `CBalloon::Draw` family, `CLabel::Draw`, `CDIB::Draw`)
are replaced with calls to a small abstract interface:

- `drawImage(...)` — composite a bitmap (backs `BitBlt`/`StretchBlt`/`CDIB::Draw`)
- `strokePath(...)` / `fillPath(...)` — balloon outlines, splines (backs
  pen/brush + `Polygon`/`Polyline`)
- `drawText(...)` — balloon text runs (backs `TextOut`)
- `measureText(...)` — **called during layout**, not just at draw time, because
  `balloon.cpp`'s line-breaking (`BreakIntoLines`, `FindFurthestLineBreak`) asks
  the DC how wide a string is. The renderer is therefore constructed *before*
  layout runs.

The core emits draw commands and text-measurement queries; it never touches an
OS API.

**Refinement from the source map** (`2026-07-17-macos-comic-only-port-source-map.md`):
a full read of the actual GDI call sites shows the seam is larger than the
initial 5 methods. Balloons are drawn as **paths** (`BeginPath`/`PolyBezierTo`/
`EndPath` → stroke/fill), so the interface needs path construction
(`beginPath/moveTo/lineTo/addCubicBezierTo/closeSubpath/endPath`), a combined
`fillAndStrokePath` (balloons fill white + stroke in one GDI call), an explicit
`FillRule` (balloon spline is open while the tail traj is closed — winding
differs), `getFontMetrics`, `fillEllipseInRect` (thought bubbles), and
`saveState/restoreState/clipToRect/concatTransform` (replacing the original's
manually-balanced, leak-prone `OffsetWindowOrg` push/pop). The full method table
lives in the source-map doc.

### Layer B — the Mac app (Objective-C++ / Cocoa)

Chosen framework: **Objective-C++ (`.mm`) + Cocoa/AppKit, rendering with Core
Graphics + Core Text.** Rationale: it keeps the C++ engine intact with *zero*
interop shim (C++ and Cocoa coexist in `.mm` translation units), and produces a
genuinely native Cocoa app (unlike Qt's bundled runtime; unlike Swift's
still-maturing C++ interop glue). Core Graphics maps cleanly onto the engine's
bitmap-composite + path-stroke + text-out draw model.

- `CoreGraphicsRenderer : IComicRenderer` — implements the interface over
  `CGContext` + Core Text; wraps `CDIB` bits into a `CGImage` once at load.
- `ComicView : NSView` — holds the current page's panels; `drawRect:` replays
  each panel's draw list through the renderer.
- `ComposerController` — the UI: text field, character picker, emotion/gesture
  picker, "Add panel" button. Feeds input to the core, appends the resulting
  panel to `ComicView`.
- `AppDelegate` / menu / window — standard AppKit shell + window geometry.

## Module layout

New folder `v1.0-pre-mac/` (existing trees untouched).

### `libcomic/` — portable C++ static library

| Unit | From | Responsibility |
|------|------|----------------|
| `geometry` | `bbox`, `vector2d`, `spline`, `splinutl`, `traj` | Rects, vectors, splines, trajectories. Pure math. |
| `art` | `dib`, `avatario` | Load `.avb` → in-memory `CDIB` (BITMAPINFO + bits). No drawing. |
| `avatar` | `avatar` | Character model: poses, emotion→gesture mapping. |
| `semantics` | `semantic`, `textpose` | Text → emotion/gesture/backdrop rules (the expert system). |
| `layout` | `panel`, `balloon` (non-draw parts) | Compose a panel: place bodies, size/shape balloons, break text into lines. Emits a draw list. |
| `IComicRenderer` | *new* | Abstract seam: `drawImage / strokePath / fillPath / drawText / measureText`. |

### `mac/` — Objective-C++ Cocoa app

| Unit | Responsibility |
|------|----------------|
| `CoreGraphicsRenderer` (`.mm`) | Implements `IComicRenderer` over `CGContext` + Core Text. |
| `ComicView : NSView` (`.mm`) | Runs each panel's draw list in `drawRect:`. |
| `ComposerController` (`.mm`) | Text field + character/emotion pickers + add-panel. |
| `AppDelegate` / menu / window | AppKit shell + window geometry. |

**Build:** Makefile or Xcode project; `libcomic` compiled as C++, `mac/` as
Objective-C++; link `Cocoa`, `CoreGraphics`, `CoreText`, `ImageIO`.

## Data flow

User types text + picks character/emotion in `ComposerController` → core runs
`semantics` (text→gesture/backdrop) → `layout` composes a `Panel` (calling back
into `measureText` for line-breaking) → emits a `PanelDrawList` →
`ComicView.drawRect:` replays it through `CoreGraphicsRenderer`.

## Error handling

`.avb` load failures surface as a missing-character alert, not a crash (original
`ASSERT`s become graceful returns). With no network there is no network error
surface at all.

## Testing

`libcomic` is OS-free, so it gets plain C++ command-line unit tests (semantics
rules, `.avb` parse, layout geometry) that run without Cocoa. The renderer is
validated visually by rendering a known line and inspecting the panel.

## Explicitly cut (YAGNI for comic-only mode)

- Networking/IRC: `irc.cpp`, `ircsock`, `tlssock`, `chatprot`, `roomlist`,
  `setupdlg`, `admindlg`, `profdlg`, `bothdlg`.
- OLE/COM/binder: `bind*.cpp`, `mfcbind`, `oleobjct`, `dumbwnd`, `ipframe`,
  `binddoc`, `binditem`, `bindview`, `bindtarg`.
- URL detection: `url.cpp`, `urlfind.cpp` (~3,900 lines).
- Printing: `print.cpp`.

## Highest-risk items (from the source map — must be de-risked early)

The per-module read surfaced risks that reorder the work: several must be probed
with a spike *before* their module is fully ported, because getting them wrong
invalidates downstream layout/render code.

1. **Y-up / TWIP coordinate convention** (highest). `bbox.cpp` uses a math Y-up
   convention (`top`=max-y); panels live in negative-Y; `CGRect` can't hold
   negative height and `NSView` defaults Y-down. Keep all layout in native
   integer `RECT` space; validate the single TWIP→points+flip `CGAffineTransform`
   with one golden-position render before writing app code.
2. **Spline pixel-parity** vs a lossy `(WORD)tension` cache key, `PI=3.14159`,
   and mixed truncation/rounding — capture golden bezier arrays from the original
   binary and assert byte-identical.
3. **1-bit mask raster-op compositing** (`MERGEPAINT`+`SRCAND`) has no
   CoreGraphics equivalent and gates whether avatars render — spike it as
   premultiplied alpha against a reference screenshot first.
4. **Measure-text-during-layout**: build the headless CoreText measurer first and
   quantify drift vs GDI `GetTextExtent`, since it drives every line break.
5. **Retina off-screen buffer sizing** (avoid right/bottom clip at scale=2).
6. **Clean extraction from IRC/OLE**: port only `CUI::Say → ShowSay → … →
   AddLine` without `serverConn.Send`; enforce with a link-time symbol check.
7. **Manual ownership / interior raw pointers** — move to index-based refs and
   `vector<unique_ptr>`; validate under AddressSanitizer.

See the source-map doc for the exact probes, the dependency-ordered 7-step build
sequence, the global-state refactor list, and per-module test targets.
