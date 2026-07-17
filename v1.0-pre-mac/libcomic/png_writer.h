// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// png_writer.h — minimal RGBA PNG encoder for headless test/debug output.
// Uses zlib (ships with macOS). Not part of the app; test-only utility.

#ifndef PNG_WRITER_H
#define PNG_WRITER_H

#include <string>
#include <vector>

#include "comic_types.h"

namespace comic {

// Write a top-down RGBA buffer (width*height*4) as a PNG. Returns false on I/O
// or compression failure.
bool writePng(const std::string& path, const std::vector<u8>& rgba, int width, int height);

} // namespace comic

#endif // PNG_WRITER_H
