// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// ComicChatMac MVP — a native Cocoa app that renders one comic panel from a
// chosen character + a typed line. No IRC, no networking. Proves the port seam
// end to end: .avb art -> CoreGraphics render -> live AppKit window.

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "CoreGraphicsRenderer.h"
#include "comic_avatar.h"
#include "comic_backdrop.h"
#include "comic_compose.h"
#include "comic_dib.h"
#include "comic_document.h"
#include "comic_page.h"
#include "comic_panel.h"

// Directory holding the .avb art + the AT_SIMPLE character list. For the MVP we
// point at the sibling v1.0-pre-modern art tree (resolved at launch).
static NSString* AvatarDir() {
    // Allow override via argv[1]; else walk up from the executable looking for
    // the sibling v1.0-pre-modern art tree.
    NSArray* args = [[NSProcessInfo processInfo] arguments];
    if (args.count > 1) return args[1];
    NSFileManager* fm = [NSFileManager defaultManager];
    NSString* dir = [args[0] stringByDeletingLastPathComponent];
    for (int i = 0; i < 8; ++i) {
        NSString* cand = [dir stringByAppendingPathComponent:@"v1.0-pre-modern/comicart/avatars"];
        BOOL isDir = NO;
        if ([fm fileExistsAtPath:cand isDirectory:&isDir] && isDir) return cand;
        dir = [dir stringByDeletingLastPathComponent];
    }
    return @"v1.0-pre-modern/comicart/avatars"; // last-ditch relative
}

// Backdrop art lives alongside the avatars, in a sibling "backdrop" dir.
static NSString* BackdropDir() {
    return [[AvatarDir() stringByDeletingLastPathComponent]
        stringByAppendingPathComponent:@"backdrop"];
}

static const char* kChars[] = {
    "connor", "glenda", "jordan", "pedagog", "rainbow", "tux", "waf",
    "anna", "armando", "bolo", "cro", "dan", "denise", "hugh", "lance",
    "lynnea", "margaret", "mike", "susan", "tiki", "tongtyed", "xeno"};

// ---------------------------------------------------------------------------
// ComicView — an accumulating comic PAGE. Holds an ordered history of committed
// panels (character body + text) and tiles them via PageLayout + the per-cell
// transform proven in render_page. Used as an NSScrollView document view; it is
// NON-flipped and grows downward as panels are appended.
// ---------------------------------------------------------------------------
struct PanelRecord {
    CGImageRef image;   // retained; released in clearPanels / dealloc
    int bodyW;
    int bodyH;
    std::string text;
    comic::SpeechMode mode;
};

@interface ComicView : NSView
@property(nonatomic, assign) CTFontRef font;
- (void)addPanelWithImage:(CGImageRef)img width:(int)w height:(int)h text:(const std::string&)text mode:(comic::SpeechMode)mode;
- (void)clearPanels;
- (int)panelCount;
// Set (or clear, with nil) the backdrop applied to ALL panels. Retained here.
- (void)setBackdropImage:(CGImageRef)img width:(int)w height:(int)h;
@end

@implementation ComicView {
    std::vector<PanelRecord> _panels;
    CGImageRef _backdrop;
    int _backdropW;
    int _backdropH;
}

- (BOOL)isFlipped { return NO; }

// cell geometry
static const int kPanelW = 300;
static const int kPanelH = 400;
static const int kGap = 12;

- (int)columns {
    CGFloat avail = self.enclosingScrollView ? self.enclosingScrollView.contentSize.width : self.bounds.size.width;
    int c = (int)((avail + kGap) / (kPanelW + kGap));
    return c < 1 ? 1 : c;
}

// NOTE: named pageLayout (not "layout") to avoid colliding with NSView's
// built-in -layout method, which returns void.
- (comic::PageLayout)pageLayout {
    return comic::PageLayout(kPanelW, kPanelH, [self columns], kGap, kGap);
}

- (void)addPanelWithImage:(CGImageRef)img width:(int)w height:(int)h text:(const std::string&)text mode:(comic::SpeechMode)mode {
    if (img) CGImageRetain(img);
    _panels.push_back(PanelRecord{img, w, h, text, mode});
    [self relayout];
}

- (void)clearPanels {
    for (auto& r : _panels) if (r.image) CGImageRelease(r.image);
    _panels.clear();
    [self relayout];
}

- (int)panelCount { return (int)_panels.size(); }

- (void)setBackdropImage:(CGImageRef)img width:(int)w height:(int)h {
    if (img) CGImageRetain(img);
    if (_backdrop) CGImageRelease(_backdrop);
    _backdrop = img;
    _backdropW = w;
    _backdropH = h;
    [self setNeedsDisplay:YES];
}

- (void)relayout {
    comic::PageLayout lay = [self pageLayout];
    int pageH = lay.pageHeight((int)_panels.size());
    CGFloat visibleH = self.enclosingScrollView ? self.enclosingScrollView.contentSize.height : self.bounds.size.height;
    CGFloat visibleW = self.enclosingScrollView ? self.enclosingScrollView.contentSize.width : self.bounds.size.width;
    CGFloat h = pageH < visibleH ? visibleH : pageH;   // never shorter than the viewport
    [self setFrame:NSMakeRect(0, 0, visibleW, h)];
    [self setNeedsDisplay:YES];
    // Reveal the newest panel. In a NON-flipped doc view, reading-order index 0
    // is drawn at the TOP (see drawRect using viewHeight - cell.bottom), which
    // is the HIGH-y end in the scroll view. The newest panel is at the LOW-y
    // (bottom) end, so scroll to y=0 to reveal it.
    // NOTE: this is the one coordinate/scroll-direction thing that may need live
    // adjustment. If the newest panel is NOT visible on a live run, try
    // [self scrollPoint:NSMakePoint(0, NSMaxY(self.bounds))] instead.
    [self scrollPoint:NSMakePoint(0, 0)];
}

- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    // Clear to a neutral gray so the white panels read as panels.
    CGContextSetRGBFillColor(ctx, 0.85, 0.85, 0.85, 1.0);
    CGContextFillRect(ctx, NSRectToCGRect(self.bounds));

    comic::PageLayout lay = [self pageLayout];
    int H = (int)self.bounds.size.height;
    for (int i = 0; i < (int)_panels.size(); ++i) {
        const PanelRecord& rec = _panels[i];
        comic::Rect cell = lay.cellRect(i);
        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, cell.left, H - cell.bottom);
        comic::CoreGraphicsRenderer renderer(ctx, kPanelH);
        comic::Panel panel(kPanelW, kPanelH, (const void*)self.font);
        if (_backdrop) {
            panel.setBackdrop((const void*)_backdrop, _backdropW, _backdropH);
        }
        if (rec.image) {
            comic::PanelBody body; body.image = (const void*)rec.image;
            body.width = rec.bodyW; body.height = rec.bodyH;
            panel.setBody(body);
        }
        panel.setText(rec.text);
        panel.setSpeechMode(rec.mode);
        panel.draw(renderer);
        CGContextRestoreGState(ctx);
    }
}

- (void)dealloc {
    for (auto& r : _panels) if (r.image) CGImageRelease(r.image);
    if (_backdrop) CGImageRelease(_backdrop);
    [super dealloc];
}
@end

// ---------------------------------------------------------------------------
// AppController — owns the window, the character picker, the say box, and the
// avatar model. Builds a CGImage from the decoded pose.
// ---------------------------------------------------------------------------
@interface AppController : NSObject <NSApplicationDelegate>
@end

@implementation AppController {
    NSWindow* _window;
    ComicView* _comic;
    NSScrollView* _scroll;
    NSPopUpButton* _picker;
    NSPopUpButton* _modePicker;
    NSPopUpButton* _backdropPicker;
    NSTextField* _say;
    NSButton* _auraToggle;
    CTFontRef _font;
    std::string _avatarDir;
    std::string _backdropDir;
    NSString* _currentName;
    // Ordered conversation history — the source of truth for save/replay. Kept
    // in lockstep with the panels appended to _comic (one DocEntry per panel).
    std::vector<comic::DocEntry> _doc;
}

- (CGImageRef)makeImageFromRGBA:(const std::vector<comic::u8>&)rgba
                          width:(int)w height:(int)h {
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef bmp = CGBitmapContextCreate(
        (void*)rgba.data(), w, h, 8, w * 4, cs,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGImageRef img = CGBitmapContextCreateImage(bmp);
    CGContextRelease(bmp);
    CGColorSpaceRelease(cs);
    return img;
}

- (void)loadCharacter:(NSString*)name {
    _currentName = [name copy];
}

- (comic::SpeechMode)selectedMode {
    NSInteger i = [_modePicker indexOfSelectedItem];
    switch (i) {
        case 1: return comic::SpeechMode::Think;
        case 2: return comic::SpeechMode::Whisper;
        case 3: return comic::SpeechMode::Shout;
        default: return comic::SpeechMode::Say;
    }
}

// Compose the body for (character, text) and append a panel to the page. This
// is the single deterministic compose path shared by live typing (commitPanel)
// and document replay (openDocument:). The backdrop and (font/frame) styling
// come from the ComicView, so a replay picks up the current backdrop just like
// live-typed panels. Returns YES if a panel was appended.
- (BOOL)renderPanelForCharacter:(const std::string&)character
                           text:(const std::string&)text
                           mode:(comic::SpeechMode)mode
                           aura:(bool)drawAura {
    auto av = comic::Avatar::load(_avatarDir, character);
    if (!av) { NSLog(@"could not load %s", character.c_str()); return NO; }
    comic::ComposedBody body = av->composeBodyForText(text, /*maskInsideIsHigh=*/true, drawAura);
    if (!body.valid()) { NSLog(@"could not compose %s", character.c_str()); return NO; }
    CGImageRef img = [self makeImageFromRGBA:body.rgba width:body.width height:body.height];
    [_comic addPanelWithImage:img width:body.width height:body.height text:text mode:mode];
    if (img) CGImageRelease(img);   // addPanel retains its own ref
    return YES;
}

- (void)commitPanel {
    if (!_currentName) return;
    NSString* line = [_say stringValue];
    if ([line length] == 0) return;
    std::string character = std::string(_currentName.UTF8String);
    std::string text = std::string(line.UTF8String);
    bool drawAura = (_auraToggle && [_auraToggle state] == NSControlStateValueOn);
    if (![self renderPanelForCharacter:character text:text mode:[self selectedMode] aura:drawAura]) return;
    // Only record the utterance once the panel actually rendered, keeping _doc
    // in lockstep with the panels on the page.
    _doc.push_back(comic::DocEntry{character, text});
    [_say setStringValue:@""];
}

- (void)pickerChanged:(id)sender {
    [self loadCharacter:[_picker titleOfSelectedItem]];
}

- (void)backdropChanged:(id)sender {
    NSString* name = [_backdropPicker titleOfSelectedItem];
    if (!name || [name isEqualToString:@"(none)"]) {
        [_comic setBackdropImage:nil width:0 height:0];
        return;
    }
    std::unique_ptr<comic::Backdrop> bd(
        comic::Backdrop::load(_backdropDir, std::string(name.UTF8String)));
    if (!bd) { NSLog(@"could not load backdrop %@", name); return; }
    CGImageRef img = [self makeImageFromRGBA:bd->rgba() width:bd->width() height:bd->height()];
    [_comic setBackdropImage:img width:bd->width() height:bd->height()];
    if (img) CGImageRelease(img); // ComicView retains its own ref
}

- (void)sayChanged:(id)sender {
    [self commitPanel];
}

// --- Save / Open ---------------------------------------------------------
// Faithful to the original Comic Chat model: we persist the ordered utterances
// (character + text), not the pixels. On open we clear the page and replay each
// utterance through the same deterministic compose path, rebuilding an
// identical comic.

- (void)saveDocument:(id)sender {
    NSSavePanel* panel = [NSSavePanel savePanel];
    [panel setNameFieldStringValue:@"conversation.ccm"];
    if ([panel runModal] != NSModalResponseOK) return;
    NSString* path = [[panel URL] path];
    if (!path) return;

    comic::Document out;
    for (const comic::DocEntry& e : _doc) out.addEntry(e.character, e.text);
    if (!out.save(std::string(path.UTF8String))) {
        NSLog(@"failed to save document to %@", path);
    }
}

- (void)openDocument:(id)sender {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setAllowsMultipleSelection:NO];
    [panel setCanChooseDirectories:NO];
    if ([panel runModal] != NSModalResponseOK) return;
    NSString* path = [[panel URL] path];
    if (!path) return;

    std::optional<comic::Document> loaded = comic::Document::load(std::string(path.UTF8String));
    if (!loaded) {
        NSLog(@"failed to load document from %@", path);
        return;
    }

    // Clear the current page and history, then replay each entry in order.
    [_comic clearPanels];
    _doc.clear();
    for (int i = 0; i < loaded->entryCount(); ++i) {
        const comic::DocEntry& e = loaded->entry(i);
        // DocEntry currently persists only (character, text); replay uses the
        // default Say balloon and no aura until those fields are added.
        if ([self renderPanelForCharacter:e.character
                                     text:e.text
                                     mode:comic::SpeechMode::Say
                                     aura:false]) {
            _doc.push_back(e);
        }
    }
}

- (void)buildMenu {
    NSMenu* mainMenu = [[NSMenu alloc] init];

    // Application menu (first item, holds Quit).
    NSMenuItem* appItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:appItem];
    NSMenu* appMenu = [[NSMenu alloc] init];
    [appMenu addItemWithTitle:@"Quit Comic Chat"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    [appItem setSubmenu:appMenu];

    // File menu with Open and Save.
    NSMenuItem* fileItem = [[NSMenuItem alloc] init];
    [mainMenu addItem:fileItem];
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    NSMenuItem* open = [fileMenu addItemWithTitle:@"Open…"
                                           action:@selector(openDocument:)
                                    keyEquivalent:@"o"];
    [open setTarget:self];
    NSMenuItem* save = [fileMenu addItemWithTitle:@"Save…"
                                           action:@selector(saveDocument:)
                                    keyEquivalent:@"s"];
    [save setTarget:self];
    [fileItem setSubmenu:fileMenu];

    [NSApp setMainMenu:mainMenu];
}

- (void)applicationDidFinishLaunching:(NSNotification*)note {
    [self buildMenu];
    _avatarDir = std::string(AvatarDir().UTF8String);
    _backdropDir = std::string(BackdropDir().UTF8String);
    _font = CTFontCreateWithName(CFSTR("Comic Sans MS"), 18.0, nullptr);
    if (!_font) _font = CTFontCreateWithName(CFSTR("Helvetica"), 18.0, nullptr);

    NSRect frame = NSMakeRect(0, 0, 460, 640);
    _window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                             NSWindowStyleMaskResizable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [_window setTitle:@"Comic Chat (Mac MVP)"];
    [_window center];

    NSView* content = [_window contentView];

    // Two control rows at the top. Row 1: character / speech-mode / backdrop
    // pickers. Row 2: the say box (plus the Aura checkbox).
    _picker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(12, frame.size.height - 36, 110, 26)];
    for (const char* c : kChars) [_picker addItemWithTitle:[NSString stringWithUTF8String:c]];
    [_picker setAutoresizingMask:NSViewMinYMargin];
    [_picker setTarget:self];
    [_picker setAction:@selector(pickerChanged:)];
    [content addSubview:_picker];

    // Speech-mode picker: selects the balloon shape (Say/Think/Whisper/Shout).
    _modePicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(128, frame.size.height - 36, 96, 26)];
    for (NSString* m in @[@"Say", @"Think", @"Whisper", @"Shout"])
        [_modePicker addItemWithTitle:m];
    [_modePicker setAutoresizingMask:NSViewMinYMargin];
    [content addSubview:_modePicker];

    // Backdrop picker: "(none)" first, then the scanned backdrop names.
    _backdropPicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(230, frame.size.height - 36, 120, 26)];
    [_backdropPicker addItemWithTitle:@"(none)"];
    for (const auto& n : comic::Backdrop::availableNames(_backdropDir))
        [_backdropPicker addItemWithTitle:[NSString stringWithUTF8String:n.c_str()]];
    [_backdropPicker setAutoresizingMask:NSViewMinYMargin];
    [_backdropPicker setTarget:self];
    [_backdropPicker setAction:@selector(backdropChanged:)];
    [content addSubview:_backdropPicker];

    // Row 2: the say box.
    _say = [[NSTextField alloc] initWithFrame:NSMakeRect(12, frame.size.height - 68, 370, 26)];
    [_say setPlaceholderString:@"Type a line and press Return…"];
    [_say setAutoresizingMask:NSViewMinYMargin | NSViewWidthSizable];
    [_say setTarget:self];
    [_say setAction:@selector(sayChanged:)];
    [content addSubview:_say];

    // "Aura" checkbox on row 2, right of the say box — when on, bodies are
    // composed with the pose's nimbus glow.
    _auraToggle = [[NSButton alloc] initWithFrame:NSMakeRect(390, frame.size.height - 66, 60, 22)];
    [_auraToggle setButtonType:NSButtonTypeSwitch];
    [_auraToggle setTitle:@"Aura"];
    [_auraToggle setState:NSControlStateValueOff];
    [_auraToggle setAutoresizingMask:NSViewMinYMargin | NSViewMinXMargin];
    [content addSubview:_auraToggle];

    _comic = [[ComicView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 80)];
    _comic.font = _font;

    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 80)];
    [_scroll setHasVerticalScroller:YES];
    [_scroll setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_scroll setDocumentView:_comic];
    [content addSubview:_scroll];

    [self loadCharacter:[_picker titleOfSelectedItem]];

    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { return YES; }
@end

int main(int argc, const char* argv[]) {
    (void)argc; (void)argv;
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        AppController* controller = [[AppController alloc] init];
        [app setDelegate:controller];
        [app run];
    }
    return 0;
}
