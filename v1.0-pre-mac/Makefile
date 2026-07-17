# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.
#
# Build for the macOS comic-only MVP.
#   make test_art   -> headless art-pipeline test (no Cocoa)
#   make app        -> the ComicChatMac.app bundle
#   make run        -> build + launch the app
#   make clean

CXX      := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -O1 -g -Ilibcomic
BUILD    := build

CORE_SRC := libcomic/comic_dib.cpp libcomic/comic_avatar.cpp libcomic/comic_compose.cpp libcomic/comic_semantics.cpp libcomic/comic_page.cpp
CORE_OBJ := $(CORE_SRC:libcomic/%.cpp=$(BUILD)/%.o)

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: libcomic/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- headless art-pipeline test -------------------------------------------
$(BUILD)/test_art: $(CORE_OBJ) $(BUILD)/png_writer.o $(BUILD)/test_art.o
	$(CXX) $(CXXFLAGS) $^ -lz -o $@

.PHONY: test_art
test_art: $(BUILD)/test_art

# --- aura/nimbus compositing unit test ------------------------------------
$(BUILD)/test_aura: $(BUILD)/comic_dib.o $(BUILD)/comic_compose.o $(BUILD)/test_aura.o
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: test_aura
test_aura: $(BUILD)/test_aura
	./$(BUILD)/test_aura

# --- Cocoa app ------------------------------------------------------------
MM_SRC   := mac/CoreGraphicsRenderer.mm mac/main.mm
MM_OBJ   := $(BUILD)/CoreGraphicsRenderer.o $(BUILD)/main.o
PANEL_OBJ := $(BUILD)/comic_panel.o
FRAMEWORKS := -framework Cocoa -framework CoreGraphics -framework CoreText

$(BUILD)/%.o: mac/%.mm | $(BUILD)
	$(CXX) $(CXXFLAGS) -Imac -c $< -o $@

APP := ComicChatMac.app
$(APP): $(CORE_OBJ) $(PANEL_OBJ) $(MM_OBJ)
	@mkdir -p $(APP)/Contents/MacOS
	$(CXX) $(CXXFLAGS) $^ $(FRAMEWORKS) -o $(APP)/Contents/MacOS/ComicChatMac
	@printf '%s\n' '<?xml version="1.0" encoding="UTF-8"?>' \
	  '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">' \
	  '<plist version="1.0"><dict>' \
	  '<key>CFBundleName</key><string>ComicChatMac</string>' \
	  '<key>CFBundleIdentifier</key><string>com.example.comicchatmac</string>' \
	  '<key>CFBundleExecutable</key><string>ComicChatMac</string>' \
	  '<key>CFBundlePackageType</key><string>APPL</string>' \
	  '<key>NSHighResolutionCapable</key><true/>' \
	  '</dict></plist>' > $(APP)/Contents/Info.plist

.PHONY: app
app: $(APP)

# --- headless full-panel render (same seam as the app) --------------------
$(BUILD)/render_panel: $(CORE_OBJ) $(PANEL_OBJ) $(BUILD)/CoreGraphicsRenderer.o $(BUILD)/render_panel.o
	$(CXX) $(CXXFLAGS) $^ -framework CoreGraphics -framework CoreText -framework ImageIO -framework Foundation -framework UniformTypeIdentifiers -o $@

.PHONY: render_panel
render_panel: $(BUILD)/render_panel

# --- headless multi-panel page render -------------------------------------
$(BUILD)/render_page: $(CORE_OBJ) $(PANEL_OBJ) $(BUILD)/CoreGraphicsRenderer.o $(BUILD)/render_page.o
	$(CXX) $(CXXFLAGS) $^ -framework CoreGraphics -framework CoreText -framework ImageIO -framework Foundation -framework UniformTypeIdentifiers -o $@

.PHONY: render_page
render_page: $(BUILD)/render_page

.PHONY: run
run: $(APP)
	./$(APP)/Contents/MacOS/ComicChatMac

.PHONY: clean
clean:
	rm -rf $(BUILD) ComicChatMac.app
