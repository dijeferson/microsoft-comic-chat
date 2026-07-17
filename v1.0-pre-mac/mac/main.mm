// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// ComicChatMac MVP — a native Cocoa app that renders one comic panel from a
// chosen character + a typed line. No IRC, no networking. Proves the port seam
// end to end: .avb art -> CoreGraphics render -> live AppKit window.

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include <memory>
#include <string>
#include <vector>

#include "CoreGraphicsRenderer.h"
#include "comic_avatar.h"
#include "comic_backdrop.h"
#include "comic_compose.h"
#include "comic_dib.h"
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
};

@interface ComicView : NSView
@property(nonatomic, assign) CTFontRef font;
- (void)addPanelWithImage:(CGImageRef)img width:(int)w height:(int)h text:(const std::string&)text;
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
    NSPopUpButton* _backdropPicker;
    NSTextField* _say;
    CTFontRef _font;
    std::string _avatarDir;
    std::string _backdropDir;
    NSString* _currentName;
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

- (void)commitPanel {
    if (!_currentName) return;
    NSString* line = [_say stringValue];
    if ([line length] == 0) return;
    auto av = comic::Avatar::load(_avatarDir, std::string(_currentName.UTF8String));
    if (!av) { NSLog(@"could not load %@", _currentName); return; }
    std::string text = std::string(line.UTF8String);
    comic::ComposedBody body = av->composeBodyForText(text, /*maskInsideIsHigh=*/true);
    if (!body.valid()) { NSLog(@"could not compose %@", _currentName); return; }
    CGImageRef img = [self makeImageFromRGBA:body.rgba width:body.width height:body.height];
    [_comic addPanelWithImage:img width:body.width height:body.height text:text];
    if (img) CGImageRelease(img);   // addPanel retains its own ref
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

- (void)applicationDidFinishLaunching:(NSNotification*)note {
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

    // Controls row at the top: character picker, backdrop picker, say box.
    _picker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(12, frame.size.height - 40, 120, 26)];
    for (const char* c : kChars) [_picker addItemWithTitle:[NSString stringWithUTF8String:c]];
    [_picker setAutoresizingMask:NSViewMinYMargin];
    [_picker setTarget:self];
    [_picker setAction:@selector(pickerChanged:)];
    [content addSubview:_picker];

    // Backdrop picker: "(none)" first, then the scanned backdrop names.
    _backdropPicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(138, frame.size.height - 40, 120, 26)];
    [_backdropPicker addItemWithTitle:@"(none)"];
    for (const auto& n : comic::Backdrop::availableNames(_backdropDir))
        [_backdropPicker addItemWithTitle:[NSString stringWithUTF8String:n.c_str()]];
    [_backdropPicker setAutoresizingMask:NSViewMinYMargin];
    [_backdropPicker setTarget:self];
    [_backdropPicker setAction:@selector(backdropChanged:)];
    [content addSubview:_backdropPicker];

    _say = [[NSTextField alloc] initWithFrame:NSMakeRect(264, frame.size.height - 40, 184, 26)];
    [_say setPlaceholderString:@"Type a line and press Return…"];
    [_say setAutoresizingMask:NSViewMinYMargin | NSViewWidthSizable];
    [_say setTarget:self];
    [_say setAction:@selector(sayChanged:)];
    [content addSubview:_say];

    _comic = [[ComicView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 48)];
    _comic.font = _font;

    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 48)];
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
