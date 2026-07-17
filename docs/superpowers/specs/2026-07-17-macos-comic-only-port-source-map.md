# Source Map: macOS comic-only port (grounding for the implementation plan)

**Date:** 2026-07-17
**Companion to:** `2026-07-17-macos-comic-only-port-design.md`
**Method:** parallel per-module reads of `v1.0-pre-modern/` + synthesis.

This document records what the actual source revealed, so the implementation
plan is anchored to the code rather than to first-glance assumptions. The design
seam ("portable core + `IComicRenderer` + CoreGraphics") survives contact with
the source; the details below refine it.

## The `IComicRenderer` seam (derived from real GDI call sites)

The abstract renderer needs more than the 5 methods first sketched. Consolidated
from actual call sites in `panel.cpp`, `balloon.cpp`, `traj.cpp`, `spline.cpp`,
`dib.cpp`, `pageview.cpp`:

| Method | Backs (GDI) | CoreGraphics impl |
|--------|-------------|-------------------|
| `beginPath/moveTo/lineTo/addCubicBezierTo/closeSubpath/endPath` | `BeginPath`/`MoveTo`/`LineTo`/`CloseFigure`/`EndPath` (traj.cpp:20-31, panel.cpp:183-190), `PolyBezierTo` (spline.cpp:301, arc.cpp:30) | `CGMutablePathRef` + `CGPathAddCurveToPoint` etc. |
| `strokePath(StrokeStyle)` | pen state via `SelectObject(&m_pen/&m_nimbusPen/&m_borderPen)` (balloon.cpp:483/510/524, panel.cpp:182,192); dash re-stroke `m_traj->Dash` (traj.cpp:45-95) | `CGContextSetLineWidth/StrokeColor/SetLineDash` + `CGContextStrokePath` |
| `fillAndStrokePath(fill, StrokeStyle, FillRule)` | `StrokeAndFillPath` fills white + strokes in one call (balloon.cpp:497/522) | `CGContextDrawPath(kCGPathFillStroke)`; **FillRule must be explicit** — balloon spline is `closed=FALSE` while traj is `m_closed=TRUE` (tail-gap winding) |
| `fillRect / fillEllipseInRect` | `Ellipse` for thought bubbles (balloon.cpp:682); `FillSolidRect` debug (droppable) | `CGContextAddEllipseInRect` + `kCGPathFillStroke` |
| `measureText(utf8,len,font) -> Size` | `GetTextExtent` ×13 sites in balloon.cpp (60/77/104/601/…); **runs during layout** inside `BreakIntoLines`/`FindFurthestLineBreak`/`AreaEstimate` | **Headless `CTLine`** (`CTLineGetTypographicBounds`) — no `CGContext`/window needed. This is the "measure without Cocoa" seam. |
| `getFontMetrics(font,&lineHeight,&ascent,&baseAdd)` | `GetTextMetrics` (balloon.cpp:972), `CFontInfo` ctor | `CTFontGetAscent/Descent/Leading` |
| `drawText(utf8,len,at,font,color,transparentBk)` | `TextOut` (balloon.cpp:538/550/708, pageview.cpp footer); `SetBkMode(TRANSPARENT)`, `SetTextAlign(TA_BOTTOM)` | `CTLineDraw`; mind baseline vs top-left convention |
| `drawImage(img,destRect,srcRect,interp)` | `StretchDIBits` (dib.cpp:627/644/662), body/backdrop/element draw (panel.cpp:152/163/173), composited panel `StretchBlt` (panel.cpp:912) | `CGContextDrawImage` + `kCGInterpolationNone` (COLORONCOLOR); indexed→RGBA with index-255→alpha |
| `saveState/restoreState`, `clipToRect`, `concatTransform/translate` | manual `OffsetWindowOrg` push/pop (balloon.cpp:489-530, **not RAII-balanced**), `IntersectClipRect` (panel.cpp:146), `SetMapMode(MM_TWIPS)`+`LPtoDP` (pageview.cpp:570) | `CGContextSaveGState`/`ClipToRect`/`ConcatCTM`; wrap in RAII to fix the unbalanced push/pop leak |

## Dependency-ordered build sequence

1. **shared-types + geometry** (`bbox`, `vector2d`, `spline`, `splinutl`, `traj`) — portable headers (`Point`/`Rect`/`SRECT`/`DPOINT`/`BEZIER`/`RGBA`); port math verbatim preserving truncation-vs-rounding semantics; GDI draw parts stubbed behind `IComicRenderer` forward-decl. *Verify:* geometry + spline golden-value unit tests, ASan-clean, zero Cocoa.
2. **art loader** (`dib`, `avatario`) — explicit little-endian field-by-field BMP reads; `ConvertToNonRLE` with `uintptr_t` alignment fix; `toRGBA()`/CGImage builder; inject `avatarDir` (no `theApp`); harden `AK_NAME` buffer + EOF guard. *Verify:* parse a real `comicart/*.avb` against a hand-decoded fixture; RLE8 decode pixel-diff.
3. **avatar** (`avatar.cpp`) — `AvatarManager` owning the `avRec`/`avatars`/`poseMap` globals; interior raw pointers → indices (realloc-safe); emotion-wheel matching preserving `EM_*` >2π gesture sentinels. *Verify:* `GetBodyFromEmotion` returns expected indices across wheel angles + sentinels.
4. **semantics** (`textpose`, `semantic`) — data-driven rule engine; reproduce `ID_RULE_*` STRINGTABLE as embedded resource (incl. empty ANGRY/SCARED/BORED); `EmotionRuleEngine` owning the rule-list singletons; `(unsigned char)` casts on all ctype calls; decide UTF-8 vs ASCII byte-heuristic policy. *Verify:* `GetEmotionsFromString` on known inputs.
5. **layout** (`balloon`, `panel` — layout paths only) — split every method into layout vs draw; layout calls only `measureText`/`getFontMetrics`; static `CPen` → `constexpr StrokeStyle`; per-panel deterministic PRNG replacing `srand`/`rand`; keep TWIP negative-Y int `RECT`. *Verify:* headless layout test with a deterministic stub measurer against golden bbox/line-break values.
6. **CoreGraphics backend + body/panel/balloon draw** — implement the seam over `CGContext`; reimplement 1-bit mask `MERGEPAINT`/`SRCAND` compositing as premultiplied alpha; off-screen `CGBitmapContext` for retained panels. *Verify:* golden-image perceptual diff vs original build; Retina scale=2 has no right/bottom clip.
7. **Cocoa app shell** — `ComicView : NSView` (flipped, in `NSScrollView`); `ComposerController` (Say/Think/Emote/Action → `ProcessLine` → `AddLine`, **without** the `serverConn.Send` half); `AssetLocator` over `NSBundle`; register Comic Sans via `CTFontManager`; `FitPanelsWide` reflow on resize. *Verify:* launch, type, paginate, resize-reflow; save/load regenerates identical comic via history replay.

## Top risks (each with a de-risking probe) — READ BEFORE PLANNING

1. **Y-up / TWIP coordinate convention** (highest). `bbox.cpp` assumes math Y-up (`top`=max-y); panels live in y∈[0,−unitHeight]; `CGRect` can't hold negative height and NSView defaults Y-down. *Probe:* keep all layout tests in native int-`RECT` space; add ONE golden render placing a balloon at a known TWIP coord and diff its pixel position — if mirrored, the single `CGAffineTransform` (points=twips/20 + flip) is wrong.
2. **Spline pixel-parity** vs lossy `(WORD)tension` cache key, `PI=3.14159`, and mixed truncation/rounding. *Probe:* capture golden bezier arrays from original `chat.exe` at default tension 0.4 (maps to key 0!) and assert byte-identical before porting balloon.
3. **1-bit mask raster-op compositing** (`MERGEPAINT` then `SRCAND`, ordered by TORSOFIRST/HEADMASK/TORSOMASK) has no CG equivalent and gates whether avatars render at all. *Probe:* early spike — one real `.avb` pose reimplemented as premultiplied alpha, PNG-diff vs original.
4. **Measure-text-during-layout with no live view.** If CoreText metrics differ from GDI `GetTextExtent`, every line break/balloon size shifts. *Probe:* build the headless `CTLine` measurer FIRST; compare against captured `GetTextExtent` values; quantify drift, set tolerance.
5. **Retina off-screen buffer sizing.** Original sizes the DIB from `LPtoDP` (not `GetDeviceCaps`) to stop high-DPI clipping and rebuilds on drift. *Probe:* golden render at scale=1 and 2; assert full panel present; verify invalidation on `backingScaleFactor` change.
6. **Extracting the pipeline from IRC/OLE.** `ChatSendText` both sends PRIVMSG and calls `ShowSay`; the thin slice is `CUI::Say → ShowSay → AddAndExecute → SayEntry::Execute → ProcessLine → AddLine` **without** `serverConn.Send`. *Probe:* trace the chain for one typed line; assert (link-time) no `irc.cpp`/`ole*` symbol is referenced by `libcomic` or the shell.
7. **Manual ownership / interior raw pointers on realloc.** `m_faceRec`/`m_torsoRec` point into malloc'd arrays; `CDIB m_bMyBits`+`ConvertToNonRLE` silently takes ownership (double-free risk). *Probe:* port to index-based refs / `vector<unique_ptr>`; run the whole suite under AddressSanitizer.

## Global state to refactor (blocks a clean library boundary)

- `theApp` + `GetAvatarDir/GetBackDropDir/GetBaseDir` → injected `AssetLocator` (NSBundle paths).
- `cui` global `CUI` + `GetView/GetChatDoc/GetClientDC` untyped back-pointers → typed refs + defined init order; `GetClientDC()` → headless measuring context.
- spline basis-matrix caches (`cardinalMatrixMap`/`betaMatrixMap`) → owned by a spline factory; fix lossy `(WORD)tension` key.
- avatar globals (`avRec`/`avatars`/`poseMap`/`strFiles`, index-0 NULL sentinel is load-bearing) → `AvatarManager`.
- static GDI objects: balloon `m_pen`/`m_nimbusPen`, panel `m_borderPen` (static-init-before-context hazard) → `constexpr StrokeStyle`.
- textpose rule singletons → `EmotionRuleEngine`.
- global `rand()`/`srand()` (per-panel `m_seed`, `srand(1515)`) → per-panel seeded PRNG instance.
- Windows path separators (`ReverseFind('\\')`) → `/` / `std::filesystem` / NSURL.
