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
#include "comic_compose.h"
#include "comic_dib.h"
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

static const char* kChars[] = {
    "connor", "glenda", "jordan", "pedagog", "rainbow", "tux", "waf",
    "anna", "armando", "bolo", "cro", "dan", "denise", "hugh", "lance",
    "lynnea", "margaret", "mike", "susan", "tiki", "tongtyed", "xeno"};

// ---------------------------------------------------------------------------
// ComicView — draws the current panel via CoreGraphicsRenderer.
// ---------------------------------------------------------------------------
@interface ComicView : NSView
@property(nonatomic, assign) CTFontRef font;
@property(nonatomic, assign) CGImageRef bodyImage;
@property(nonatomic, assign) int bodyW;
@property(nonatomic, assign) int bodyH;
@property(nonatomic, copy) NSString* text;
@end

@implementation ComicView

- (BOOL)isFlipped { return NO; }

- (void)drawRect:(NSRect)dirtyRect {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    // Clear to a neutral gray so the white panel reads as a panel.
    CGContextSetRGBFillColor(ctx, 0.85, 0.85, 0.85, 1.0);
    CGContextFillRect(ctx, NSRectToCGRect(self.bounds));

    int w = (int)self.bounds.size.width;
    int h = (int)self.bounds.size.height;

    comic::CoreGraphicsRenderer renderer(ctx, h);
    comic::Panel panel(w, h, (const void*)self.font);
    if (self.bodyImage) {
        comic::PanelBody body;
        body.image = (const void*)self.bodyImage;
        body.width = self.bodyW;
        body.height = self.bodyH;
        panel.setBody(body);
    }
    panel.setText(self.text ? std::string(self.text.UTF8String) : std::string());
    panel.draw(renderer);
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
    NSPopUpButton* _picker;
    NSTextField* _say;
    CTFontRef _font;
    std::string _avatarDir;
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
    [self recompose];
}

- (void)recompose {
    // On any failure, clear the current figure so a failed pick never leaves a
    // stale character on screen under the new selection.
    auto fail = [&](NSString* why) {
        NSLog(@"%@ %@", why, _currentName);
        if (_comic.bodyImage) { CGImageRelease(_comic.bodyImage); _comic.bodyImage = NULL; }
        _comic.bodyW = _comic.bodyH = 0;
        [_comic setNeedsDisplay:YES];
    };
    if (!_currentName) { fail(@"no character"); return; }
    auto av = comic::Avatar::load(_avatarDir, std::string(_currentName.UTF8String));
    if (!av) { fail(@"could not load"); return; }
    std::string text = _comic.text ? std::string(_comic.text.UTF8String) : std::string();
    comic::ComposedBody body = av->composeBodyForText(text, /*maskInsideIsHigh=*/true);
    if (!body.valid()) { fail(@"could not compose"); return; }
    if (_comic.bodyImage) CGImageRelease(_comic.bodyImage);
    _comic.bodyImage = [self makeImageFromRGBA:body.rgba width:body.width height:body.height];
    _comic.bodyW = body.width;
    _comic.bodyH = body.height;
    [_comic setNeedsDisplay:YES];
}

- (void)pickerChanged:(id)sender {
    [self loadCharacter:[_picker titleOfSelectedItem]];
}

- (void)sayChanged:(id)sender {
    _comic.text = [_say stringValue];
    [self recompose];
}

- (void)applicationDidFinishLaunching:(NSNotification*)note {
    _avatarDir = std::string(AvatarDir().UTF8String);
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

    // Controls row at the top.
    _picker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(12, frame.size.height - 40, 140, 26)];
    for (const char* c : kChars) [_picker addItemWithTitle:[NSString stringWithUTF8String:c]];
    [_picker setAutoresizingMask:NSViewMinYMargin];
    [_picker setTarget:self];
    [_picker setAction:@selector(pickerChanged:)];
    [content addSubview:_picker];

    _say = [[NSTextField alloc] initWithFrame:NSMakeRect(160, frame.size.height - 40, 288, 26)];
    [_say setPlaceholderString:@"Type a line and press Return…"];
    [_say setAutoresizingMask:NSViewMinYMargin | NSViewWidthSizable];
    [_say setTarget:self];
    [_say setAction:@selector(sayChanged:)];
    [content addSubview:_say];

    _comic = [[ComicView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height - 48)];
    [_comic setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    _comic.font = _font;
    _comic.text = @"Hello from Comic Chat on macOS!";
    [content addSubview:_comic];

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
