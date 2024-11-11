/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

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

    fTextInfo = new text_info;
    fTextInfo->text_map = new std::map<int32, text_data>;
    fTextInfo->markup_stack = new std::vector<text_data>;
}

EditorTextView::~EditorTextView() {
    RemoveSelf();
    delete fMarkdownStyler;
    delete fTextInfo;
}

void EditorTextView::SetText(const char* text, const text_run_array* runs) {
    BTextView::SetText(text, runs);
    MarkupText(0, TextLength());
    UpdateStatus();
}

void EditorTextView::SetText(BFile* file, int32 offset, size_t size) {
    BTextView::SetText(file, offset, size);
    MarkupText(offset, TextLength());
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

void EditorTextView::MouseDown(BPoint where) {
    BTextView::MouseDown(where);
    int32 offset = OffsetAt(where);
    UpdateStatus();

    text_data *info = GetTextInfoAround(offset);
    if (info == NULL) {
        printf("no text info @%d\n", offset);
        return;
    }

    printf("got text data at offset %ul with search offset %d:\nmarkup class %d, block type %d, span type %d, text type %d\n",
            info->offset, offset, info->markup_class,
            info->markup_type.block_type, info->markup_type.span_type, info->markup_type.text_type);
}

void EditorTextView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) {
    BTextView::MouseMoved(where, code, dragMessage);

    int32 offset = OffsetAt(where);
    UpdateStatus();
}

text_data *EditorTextView::GetTextInfoAround(int32 offset) {
    for (auto info = fTextInfo->text_map->begin();  info != fTextInfo->text_map->end(); info++) {
        int32 mapOffset = info->first;

        if (mapOffset >= offset && offset <= mapOffset + info->second.length) {
            printf("found offset %d\n", mapOffset);
            return &info->second;
        }
    }
    return NULL;
}

void EditorTextView::ClearTextInfo(int32 start, int32 end) {
    text_data *data_start = GetTextInfoAround(start);
    if (data_start == NULL) return; // no text info!
    text_data *data_end = GetTextInfoAround(end);
    if (data_end == NULL) return;   // bogus, no text info!

    int32 offsetStart = data_start->offset;
    int32 offsetEnd   = data_end->offset;

    printf("updating map offsets %d - %d\n", offsetStart, offsetEnd);

    int32 offset = offsetStart;
    for (auto data : *fTextInfo->text_map) {
        if (data.first < offsetStart) {
            printf("skipping index @%d < start %d\n", offset, offsetStart);
            continue;
        } else if (data.first > offsetEnd) {
            printf("index @%d > end %d, done.\n", offset, offsetEnd);
            break;
        }
        printf("removing outdated textinfo @%d\n", offset);
        fTextInfo->text_map->erase(offset);
    }
}

// interaction with MarkupStyler - should become its own class later
void EditorTextView::MarkupText(int32 start, int32 end) {
    if (end == -1) {
        end = TextLength();
    }
    int32 size = end - start;
    char text[size + 1];

    printf("markup text %d - %d\n", start, end);
    ClearTextInfo(start, end);

    GetText(start, size, text);
    fMarkdownStyler->MarkupText(text, size, fTextInfo);

    printf("received %zu markup text blocks.\n", fTextInfo->text_map->size());

    int32 offset = 0; // workaround for MD4C anomaly
    bool isLink;    //test
    bool isCode;    //test
    const rgb_color linkColor = ui_color(B_LINK_TEXT_COLOR);
    const rgb_color codeColor = ui_color(B_SHADOW_COLOR);
    const rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);

    printf("styling...\n");

    for (auto info = fTextInfo->text_map->begin();  info != fTextInfo->text_map->end(); info++) {
        int32 mapOffset = info->first;
        printf("got offset %d\n", mapOffset);

        if (mapOffset < size) {
            if (mapOffset> offset) {
                offset = mapOffset;
            } else {
                if (mapOffset > 0) {
                    // verbatim inline blocks sometimes have a relative index ?!
                    offset += mapOffset;
                    printf("    ~bogus offset %d, fixing to %d\n", mapOffset, offset);
                }
            }
        }

        auto userData = info->second;
        const char* markupType;
        switch (userData.markup_class) {
            case MARKUP_BLOCK: {
                markupType = "BLOCK";
                if (userData.markup_type.block_type == MD_BLOCK_CODE) {
                    isCode = true;
                    printf("  got block code...\n");

                    MD_BLOCK_CODE_DETAIL *detail = (MD_BLOCK_CODE_DETAIL*) userData.detail;
                    BString info(detail->info.text, detail->info.size); info << '\0';
                    BString lang(detail->lang.text, detail->lang.size); lang << '\0';
                    printf("  got block code with info %s, language %s\n", info.String(), lang.String());
                }
                break;
            }
            case MARKUP_SPAN: {
                markupType = "SPAN";
                switch (userData.markup_type.span_type) {
                    case MD_SPAN_A: {
                        isLink = true;
                        MD_SPAN_A_DETAIL *detail = (MD_SPAN_A_DETAIL*) userData.detail;
                        BString title(detail->title.text, detail->title.size); title << '\0';
                        BString href(detail->href.text, detail->href.size); href << '\0';

                        printf("  got link with title: %s, href: <%s>\n", title.String(), href.String());
                        break;
                    }
                    case MD_SPAN_WIKILINK: {
                        isLink = true;
                        MD_SPAN_WIKILINK_DETAIL *detail = (MD_SPAN_WIKILINK_DETAIL*) userData.detail;
                        BString target(detail->target.text, detail->target.size); target << '\0';
                        printf("  got wiki link with target: %s\n", target.String());

                        break;
                    }
                    case MD_SPAN_CODE: {
                        isCode = true;
                        printf("  got code span\n");
                        break;
                    }
                    default:
                        printf("  span type %d not handled yet.\n", userData.markup_type.span_type);
                }
                break;
            }
            case MARKUP_TEXT: {
                markupType = "TEXT";
                if (isLink) {
                    SetFontAndColor(offset, offset + userData.length,
                                               fLinkFont, B_FONT_ALL, &linkColor);
                    isLink = false;
                } else if (isCode) {
                    SetFontAndColor(offset, offset + userData.length,
                                               fCodeFont, B_FONT_ALL, &codeColor);
                    isCode = false;
                } else {
                    SetFontAndColor(offset, offset + userData.length,
                                               be_plain_font, B_FONT_ALL);
                }
                break;
            }
            default: { markupType = "UNKNOWN"; }
        }

        printf("#%d: <%s> @ %d - %d\n",
            mapOffset, markupType,
            offset,
            offset + userData.length);
    }
}
