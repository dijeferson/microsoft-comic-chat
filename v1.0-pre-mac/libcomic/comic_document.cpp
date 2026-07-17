// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// See comic_document.h for the format rationale.
//
// On-disk format (human readable, robust to arbitrary text bytes):
//
//   COMICCHATDOC 1\n                 <- magic + version
//   ENTRIES <n>\n                    <- entry count
//   CHAR <len>\n<len bytes>\n        <- character name, length-prefixed
//   TEXT <len>\n<len bytes>\n        <- utterance text, length-prefixed
//   ... (CHAR/TEXT pair repeated <n> times) ...
//
// The length prefix on each field means the raw bytes are copied verbatim:
// spaces, punctuation, quotes, and embedded newlines all round-trip exactly,
// with no escaping and therefore no escaping ambiguity. A trailing '\n' after
// each field's bytes keeps the file line-friendly for humans/diffing but is
// verified on load (it must be present), so truncated/garbled files are
// rejected rather than silently misparsed.

#include "comic_document.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace comic {

static const char* kMagic = "COMICCHATDOC";
static const int kVersion = 1;

bool Document::save(const std::string& path) const {
    std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out) return false;

    out << kMagic << ' ' << kVersion << '\n';
    out << "ENTRIES " << entries_.size() << '\n';
    for (const DocEntry& e : entries_) {
        out << "CHAR " << e.character.size() << '\n';
        out.write(e.character.data(), static_cast<std::streamsize>(e.character.size()));
        out << '\n';
        out << "TEXT " << e.text.size() << '\n';
        out.write(e.text.data(), static_cast<std::streamsize>(e.text.size()));
        out << '\n';
    }
    out.flush();
    return static_cast<bool>(out);
}

namespace {

// A tolerant cursor over the file contents. Every accessor is bounds-checked;
// on any inconsistency the caller returns nullopt.
struct Parser {
    const std::string& s;
    size_t pos = 0;
    explicit Parser(const std::string& str) : s(str) {}

    // Read up to and including the next '\n'; returns the line WITHOUT the '\n'.
    // Fails if there is no newline before EOF.
    bool readLine(std::string& out) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string::npos) return false;
        out.assign(s, pos, nl - pos);
        pos = nl + 1;
        return true;
    }

    // Read exactly `len` raw bytes, then require a single trailing '\n'.
    bool readField(size_t len, std::string& out) {
        if (pos + len > s.size()) return false;
        out.assign(s, pos, len);
        pos += len;
        if (pos >= s.size() || s[pos] != '\n') return false;
        pos += 1;
        return true;
    }
};

// Parse "<keyword> <non-negative-integer>" exactly. Returns false on mismatch.
bool parseKeyedCount(const std::string& line, const std::string& keyword, size_t& value) {
    if (line.size() < keyword.size() + 1) return false;
    if (line.compare(0, keyword.size(), keyword) != 0) return false;
    if (line[keyword.size()] != ' ') return false;
    const std::string num = line.substr(keyword.size() + 1);
    if (num.empty()) return false;
    for (char c : num) {
        if (c < '0' || c > '9') return false;
    }
    // Use a stream to avoid overflow surprises; reject anything trailing.
    std::istringstream iss(num);
    unsigned long long v = 0;
    iss >> v;
    if (!iss || !iss.eof()) return false;
    value = static_cast<size_t>(v);
    return true;
}

} // namespace

std::optional<Document> Document::load(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return std::nullopt;

    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in && !in.eof()) return std::nullopt;
    std::string contents = ss.str();

    Parser p(contents);

    // Header: "COMICCHATDOC <version>".
    std::string header;
    if (!p.readLine(header)) return std::nullopt;
    {
        const std::string prefix = std::string(kMagic) + " ";
        if (header.size() < prefix.size()) return std::nullopt;
        if (header.compare(0, prefix.size(), prefix) != 0) return std::nullopt;
        std::istringstream iss(header.substr(prefix.size()));
        int version = -1;
        iss >> version;
        if (!iss || !iss.eof()) return std::nullopt;
        if (version != kVersion) return std::nullopt;
    }

    // Entry count.
    std::string countLine;
    if (!p.readLine(countLine)) return std::nullopt;
    size_t count = 0;
    if (!parseKeyedCount(countLine, "ENTRIES", count)) return std::nullopt;

    Document doc;
    for (size_t i = 0; i < count; ++i) {
        std::string charHdr, textHdr;
        size_t charLen = 0, textLen = 0;
        DocEntry e;

        if (!p.readLine(charHdr)) return std::nullopt;
        if (!parseKeyedCount(charHdr, "CHAR", charLen)) return std::nullopt;
        if (!p.readField(charLen, e.character)) return std::nullopt;

        if (!p.readLine(textHdr)) return std::nullopt;
        if (!parseKeyedCount(textHdr, "TEXT", textLen)) return std::nullopt;
        if (!p.readField(textLen, e.text)) return std::nullopt;

        doc.entries_.push_back(std::move(e));
    }

    return doc;
}

} // namespace comic
