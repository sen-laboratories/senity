/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

//#include <ObjectList.h>
#include <Messenger.h>
#include <stdio.h>

#include "EditorTextView.h"

EditorTextView::EditorTextView(BRect viewFrame, BRect textBounds, StatusBar *statusBar, BHandler *editorHandler)
: BTextView(viewFrame, "textview", textBounds,
            B_FOLLOW_ALL, B_FRAME_EVENTS | B_WILL_DRAW)
{
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());

    fStatusBar = statusBar;
    fMessenger = new BMessenger(editorHandler);

    // setup fonts
    fLinkFont = new BFont(be_plain_font);
    fLinkFont->SetFace(B_UNDERSCORE_FACE);
    fCodeFont = new BFont(be_fixed_font);

    // setup markdown syntax styler
    fMarkdownStyler = new MarkdownStyler();
    fMarkdownStyler->Init();
}

EditorTextView::~EditorTextView() {
    RemoveSelf();
    delete fMarkdownStyler;
    // later: clean helper structures
}

void EditorTextView::SetText(BFile* file, size_t size) {
    BTextView::SetText(file, 0, size);
    MarkupText(0, size);

    UpdateStatus();
}

void EditorTextView::UpdateStatus() {
    int32 start, end, line;
    GetSelection(&start, &end);
    line = CurrentLine();
    fStatusBar->UpdatePosition(end, CurrentLine(), end - OffsetAt(line));
    fStatusBar->UpdateSelection(start, end);
}

// hook methods
void EditorTextView::DeleteText(int32 start, int32 finish) {
    BTextView::DeleteText(start, finish);

    int32 from = OffsetAt(LineAt(start));       // extend back to start of line
    int32 to   = OffsetAt(LineAt(start) + 1);   // extend end offset to start of next line
    MarkupText(from, to);
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset,
                                const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);

    int32 start = OffsetAt(LineAt(offset));             // extend back to start of line from insert offset
    int32 end = OffsetAt(LineAt(start + length) + 1);   // extend end offset to start of next line
    MarkupText(start, end);
}

void EditorTextView::KeyDown(const char* bytes, int32 numBytes) {
    BTextView::KeyDown(bytes, numBytes);

    UpdateStatus();
    int32 start, end;
    GetSelection(&start, &end);

    int32 from = OffsetAt(LineAt(start));         // extend back to start of line from insert offset
    int32 to   = OffsetAt(LineAt(end) + 1);       // extend end offset to start of next line
    MarkupText(start, to);
}

// interaction with MarkupStyler - should become its own class later
void EditorTextView::MarkupText(int32 start, int32 end) {
    if (end == -1) {
        end = TextLength();
    }
    int32 size = end - start;
    char text[size + 1];
    BObjectList<text_data>* modes = new BObjectList<text_data>();

    GetText(0, size, text);
    fMarkdownStyler->MarkupText(text, size, modes);

    printf("received %d markup text blocks.\n", modes->CountItems());

    text_data* userData;
    int32 offset = 0; // workaround for MD4C anomaly
    bool isLink;    //test
    bool isCode;    //test
    const rgb_color linkColor = ui_color(B_LINK_TEXT_COLOR);
    const rgb_color codeColor = ui_color(B_SHADOW_COLOR);
    const rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);

    for (int i = 0; i < modes->CountItems(); i++) {
        userData = modes->ItemAt(i);
        if (userData->offset < size) {
            if (userData->offset > offset) {
                offset = userData->offset;
            } else {
                if (userData->offset > 0) {
                    // verbatim inline blocks sometimes have a relative index ?!
                    offset += userData->offset;
                    printf("    ~bogus offset %d, fixing to %d\n", userData->offset, offset);
                }
            }
        }

        const char* markupType;
        switch (userData->markup_type) {
            case MARKUP_BLOCK: {
                markupType = "BLOCK";
                if (userData->block_type == MD_BLOCK_CODE) {
                    isCode = true;
                    printf("  got block code...\n");

                    MD_BLOCK_CODE_DETAIL *detail = (MD_BLOCK_CODE_DETAIL*) userData->detail;
                    BString info(detail->info.text, detail->info.size); info << '\0';
                    BString lang(detail->lang.text, detail->lang.size); lang << '\0';
                    printf("  got block code with info %s, language %s\n", info.String(), lang.String());
                }
                break;
            }
            case MARKUP_SPAN: {
                markupType = "SPAN";
                if (userData->span_type == MD_SPAN_A) {
                    isLink = true;
                    MD_SPAN_A_DETAIL *detail = (MD_SPAN_A_DETAIL*) userData->detail;
                    BString title(detail->title.text, detail->title.size); title << '\0';
                    BString href(detail->href.text, detail->href.size); href << '\0';

                    printf("  got link with title: %s, href: <%s>\n", title.String(), href.String());
                } else if (userData->span_type == MD_SPAN_WIKILINK) {
                    isLink = true;
                    MD_SPAN_WIKILINK_DETAIL *detail = (MD_SPAN_WIKILINK_DETAIL*) userData->detail;
                    BString target(detail->target.text, detail->target.size); target << '\0';
                    printf("  got wiki link with target: %s\n", target.String());
                } else if (userData->span_type == MD_SPAN_CODE) {
                    isCode = true;
                    printf("  got code span\n");
                }
                break;
            }
            case MARKUP_TEXT: {
                markupType = "TEXT";
                if (isLink) {
                    SetFontAndColor(offset, offset + userData->length,
                                               fLinkFont, B_FONT_ALL, &linkColor);
                    isLink = false;
                } else if (isCode) {
                    SetFontAndColor(offset, offset + userData->length,
                                               fCodeFont, B_FONT_ALL, &codeColor);
                    isCode = false;
                }
                break;
            }
            default: { markupType = "UNKNOWN"; }
        }

        printf("#%d: <%s> @ %d - %d\n",
            i, markupType,
            offset,
            offset + userData->length);
    }
}
