/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#ifndef EDITOR_TEXT_VIEW_H
#define EDITOR_TEXT_VIEW_H

#include "MarkdownParser.h"
#include "StatusBar.h"
#include "SyntaxHighlighter.h"

#include <TextView.h>
#include <Handler.h>
#include <map>

class BFile;
class BRegion;

struct text_highlight {
    int32 startOffset;
    int32 endOffset;
    const rgb_color* fgColor;
    const rgb_color* bgColor;
    BRegion* region;
    bool generated;
    bool outline;
};

class EditorTextView : public BTextView {
public:
    EditorTextView(StatusBar* statusView, BHandler* editorHandler);
    virtual ~EditorTextView();

    virtual void AttachedToWindow();
    virtual void Draw(BRect updateRect);
    virtual void SetText(const char* text, const text_run_array* runs = nullptr);
    virtual void DeleteText(int32 start, int32 finish);
    virtual void InsertText(const char* text, int32 length, int32 offset,
                           const text_run_array* runs = nullptr);
    virtual void MessageReceived(BMessage* message);
    virtual void MouseDown(BPoint where);
    virtual	void Pulse();

    void SetText(BFile* file, int32 offset, size_t size);
    void MarkupText(const char* text);

    void HighlightSelection(const rgb_color* fgColor = nullptr,
                           const rgb_color* bgColor = nullptr,
                           bool generated = false,
                           bool outline = false);
    void Highlight(int32 startOffset, int32 endOffset,
                  const rgb_color* fgColor = nullptr,
                  const rgb_color* bgColor = nullptr,
                  bool generated = false,
                  bool outline = false);
    void ClearHighlights();
    void RedrawHighlight(text_highlight* highlight);

    bool GetOutlineAt(int32 offset, BMessage* outline, bool withNames = true);
    BMessage* GetDocumentOutline(bool withNames = true, bool withDetails = false);

private:
    void    ApplyStyles(int32 offset, int32 length);
    void    UpdateStatus();
    int32   FindBlockStart(int32 line) const;
    int32   FindBlockEnd(int32 line) const;

    void    BuildContextMenu();
    void    BuildContextSelectionMenu();

    MarkdownParser*     fMarkdownParser;
    SyntaxHighlighter*  fSyntaxHighlighter;
    BHandler*           fEditorHandler;
    StatusBar*          fStatusBar;
    BFont*              fTextFont;
    BFont*              fLinkFont;
    BFont*              fCodeFont;
    BFont*              fTableFont;
    BFont*              fTableHeaderFont;
    std::map<int32, text_highlight*>* fTextHighlights;
    int32               fOldOffset;
};

#endif // EDITOR_TEXT_VIEW_H
