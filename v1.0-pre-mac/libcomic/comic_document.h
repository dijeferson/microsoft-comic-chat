// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// comic_document.{h,cpp} — a portable, OS-free model of a saved comic
// conversation. Mirrors the original Comic Chat "store the utterances, replay
// to rebuild" model (CChatDoc::ExecuteHistory): rather than persisting pixels,
// we persist the ordered list of (character, text) utterances. Because
// Avatar::composeBodyForText is deterministic, replaying each entry through the
// same compose path reconstructs the identical comic page.
//
// The on-disk format is a small, human-readable, line-based text format with a
// magic + version header. Each entry's fields are length-prefixed so text
// containing spaces, punctuation, quotes, and embedded newlines survives a
// round trip unambiguously (no escaping ambiguity).

#ifndef COMIC_DOCUMENT_H
#define COMIC_DOCUMENT_H

#include <optional>
#include <string>
#include <vector>

namespace comic {

// One utterance: a character name + the text the user typed. (A speech-mode
// field can be added later; for now the panel is fully regenerable from these
// two fields via Avatar::composeBodyForText.)
struct DocEntry {
    std::string character;
    std::string text;
};

class Document {
public:
    Document() = default;

    void addEntry(const std::string& character, const std::string& text) {
        entries_.push_back(DocEntry{character, text});
    }
    void clear() { entries_.clear(); }

    int entryCount() const { return static_cast<int>(entries_.size()); }
    const DocEntry& entry(int i) const { return entries_[i]; }
    const std::vector<DocEntry>& entries() const { return entries_; }

    // Serialize to `path`. Returns false on any I/O failure.
    bool save(const std::string& path) const;

    // Parse a document from `path`. Returns nullopt on a missing file or any
    // malformed/corrupt content; never throws or crashes on bad input.
    static std::optional<Document> load(const std::string& path);

private:
    std::vector<DocEntry> entries_;
};

} // namespace comic

#endif // COMIC_DOCUMENT_H
