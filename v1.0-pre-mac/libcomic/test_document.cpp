// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Round-trip test for comic::Document save/load, plus tolerance of bad input.

#include <cassert>
#include <cstdio>
#include <fstream>
#include <optional>
#include <string>

#include "comic_document.h"

using namespace comic;

int main() {
    const std::string path = "build/doc_test.ccm";

    // Build a document with tricky text: a plain word, a phrase with a space,
    // punctuation, an embedded double-quote, and an embedded newline.
    Document doc;
    doc.addEntry("connor", "Hello world");
    doc.addEntry("glenda", "Well, hello there! How's it going?");
    doc.addEntry("tux", "She said \"hi\" to me.");
    doc.addEntry("jordan", "line one\nline two\nline three");
    doc.addEntry("waf", ""); // empty text is legal
    doc.addEntry("", "empty character name is legal too");

    assert(doc.save(path));

    // Reload and assert byte-identical round trip.
    std::optional<Document> loaded = Document::load(path);
    assert(loaded.has_value());
    assert(loaded->entryCount() == doc.entryCount());
    for (int i = 0; i < doc.entryCount(); ++i) {
        assert(loaded->entry(i).character == doc.entry(i).character);
        assert(loaded->entry(i).text == doc.entry(i).text);
    }

    // Nonexistent file -> nullopt, no crash.
    assert(!Document::load("build/does_not_exist_xyz.ccm").has_value());

    // Garbage file -> nullopt, no crash.
    {
        const std::string garbagePath = "build/doc_garbage.ccm";
        std::ofstream g(garbagePath, std::ios::binary | std::ios::trunc);
        g << "this is not a comic chat document\x00\x01\x02 random bytes";
        g.close();
        assert(!Document::load(garbagePath).has_value());
    }

    // Well-formed header but truncated body (CHAR len exceeds available bytes)
    // -> nullopt.
    {
        const std::string truncPath = "build/doc_trunc.ccm";
        std::ofstream t(truncPath, std::ios::binary | std::ios::trunc);
        t << "COMICCHATDOC 1\nENTRIES 1\nCHAR 100\nshort\n";
        t.close();
        assert(!Document::load(truncPath).has_value());
    }

    // Wrong version -> nullopt.
    {
        const std::string verPath = "build/doc_ver.ccm";
        std::ofstream v(verPath, std::ios::binary | std::ios::trunc);
        v << "COMICCHATDOC 999\nENTRIES 0\n";
        v.close();
        assert(!Document::load(verPath).has_value());
    }

    // Empty document round trip.
    {
        Document empty;
        const std::string emptyPath = "build/doc_empty.ccm";
        assert(empty.save(emptyPath));
        std::optional<Document> back = Document::load(emptyPath);
        assert(back.has_value());
        assert(back->entryCount() == 0);
    }

    std::printf("test_document OK (%d entries round-tripped)\n", doc.entryCount());
    return 0;
}
