/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <Messenger.h>
#include <stdio.h>

#include "EditorTextView.h"

const rgb_color linkColor = ui_color(B_LINK_TEXT_COLOR);
const rgb_color codeColor = ui_color(B_SHADOW_COLOR);
const rgb_color textColor = ui_color(B_DOCUMENT_TEXT_COLOR);

EditorTextView::EditorTextView(BRect viewFrame, BRect textBounds, StatusBar *statusBar, BHandler *editorHandler)
: BTextView(viewFrame, "textview", textBounds, B_FOLLOW_ALL, B_FRAME_EVENTS | B_WILL_DRAW)
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
    fTextInfo->text_map = new std::map<uint, text_data>;
    fTextInfo->markup_stack = new std::vector<text_data>;
}

EditorTextView::~EditorTextView() {
    RemoveSelf();
    delete fMarkdownStyler;
    delete fTextInfo;
    delete fMessenger;
}

void EditorTextView::AttachedToWindow() {
    BTextView::AttachedToWindow();
    DoLayout();
}

void EditorTextView::SetText(const char* text, const text_run_array* runs) {
    BTextView::SetText(text, runs);
    MarkupText(0, TextLength());
    DoLayout();
    UpdateStatus();
}

void EditorTextView::SetText(BFile* file, int32 offset, size_t size) {
    BTextView::SetText(file, offset, size);
    MarkupText(offset, TextLength());
    DoLayout();
    UpdateStatus();
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
}

void EditorTextView::MouseDown(BPoint where) {
    BTextView::MouseDown(where);
    //int32 offset = OffsetAt(where);
    UpdateStatus();
}

void EditorTextView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) {
    BTextView::MouseMoved(where, code, dragMessage);
}

void EditorTextView::UpdateStatus() {
    int32 start, end, line;
    GetSelection(&start, &end);
    line = CurrentLine();
    fStatusBar->UpdatePosition(end, CurrentLine(), end - OffsetAt(line));
    fStatusBar->UpdateSelection(start, end);

    // update outline in status from block / span info contained in text info stack
    text_data *info = GetTextInfoAround(end);
    if (info == NULL) {
        printf("no text info at offset %d\n", end);
        return;
    }
    BMessage* outlineItems = fMarkdownStyler->GetMarkupStack(info);
    outlineItems->PrintToStream();
    fStatusBar->UpdateOutline(outlineItems);
}

text_data *EditorTextView::GetTextInfoAround(int32 offset) {
    for (auto info : *fTextInfo->text_map) {
        int32 mapOffset = info.first;

        if (offset >= mapOffset && offset <= mapOffset + info.second.length) {
            printf("found text info @ %d for search offset %d\n", mapOffset, offset);
            return &fTextInfo->text_map->find(mapOffset)->second;
        }
    }
    return NULL;
}

void EditorTextView::ClearTextInfo(int32 start, int32 end) {
    // optimize clear all case
    if (start <= 1 && end >= TextLength()) {
        fTextInfo->text_map->clear();
        fTextInfo->markup_stack->clear();
        return;
    }

    text_data *data_start = GetTextInfoAround(start);
    if (data_start == NULL) return; // no text info!
    text_data *data_end = GetTextInfoAround(end);
    if (data_end == NULL) return;   // bogus, no text info!

    int32 offsetStart = data_start->offset;
    int32 offsetEnd   = data_end->offset;

    printf("updating map offsets %d - %d\n", offsetStart, offsetEnd);

    int32 offset = offsetStart;
    for (auto data : *fTextInfo->text_map) {
        if (data.first < offset) {
            printf("skipping index @%d < start %d\n", offset, offsetStart);
            continue;
        }
        if (offset > offsetEnd) {
            printf("index @%d > end %d, done.\n", offset, offsetEnd);
            break;
        }
        printf("removing outdated textinfo @%d\n", offset);
        fTextInfo->text_map->erase(offset);
        offset = data.first + 1;
    }
}

// interaction with MarkupStyler - should become its own class later
void EditorTextView::MarkupText(int32 start, int32 end) {
    if (end == -1) {
        end = TextLength();
    }
    int32 size = end - start;
    printf("markup text %d - %d\n", start, end);
    ClearTextInfo(start, end);

    char text[size + 1];
    GetText(start, size, text);
    fMarkdownStyler->MarkupText(text, size, fTextInfo);

    printf("\n*** received %zu markup text blocks, styling... ***\n", fTextInfo->text_map->size());

    for (auto info : *fTextInfo->text_map) {
        // &info.first is the offset used as map key and for editor lookup on select/position
        // for easier processing, it is also contained in &info.second and can be ignored here.
        StyleText(&info.second);
    }
}

void EditorTextView::StyleText(text_data *userData) {
    BFont font;
    rgb_color color;

    CalcStyle(userData->markup_stack, &font, &color);
    SetFontAndColor(userData->offset, userData->offset + userData->length, &font, B_FONT_FAMILY_AND_STYLE, &color);

    printf("[%d + %d] calc style: %u\n", userData->offset, userData->length, font.FamilyAndStyle());
}

void EditorTextView::CalcStyle(std::vector<text_data> *markup_stack, BFont *font, rgb_color *color) {
    for (auto stack_item : *markup_stack) {
        switch (stack_item.markup_class) {
            case MARKUP_BLOCK: {
                switch (stack_item.markup_type.block_type) {
                    case MD_BLOCK_CODE: {
                        font = fCodeFont;
                        *color = codeColor;
                        break;
                    }
                    case MD_BLOCK_H: {
                        BMessage *detail = fMarkdownStyler->GetDetailForBlockType(MD_BLOCK_H, stack_item.detail);
                        uint8 level;
                        if (detail->FindUInt8("level", &level) == B_OK) {
                            float headerSizeFac = (7 - level) / 3.2;                // max 6 levels in markdown
                            font->SetSize(be_plain_font->Size() * headerSizeFac);   // H1 = 2*normal size
                            if (level == 1) {
                                font->SetFace(B_OUTLINED_FACE);
                            } else {
                                font->SetFace(B_HEAVY_FACE);
                            }
                            *color = codeColor;
                        }
                        break;
                    }
                    case MD_BLOCK_QUOTE: {
                        font->SetFace(B_ITALIC_FACE);
                        *color = codeColor;
                        break;
                    }
                    case MD_BLOCK_HR:
                        font->SetFace(B_HEAVY_FACE);
                        *color = codeColor;
                        break;
                    case MD_BLOCK_HTML: {
                        font->SetSpacing(B_FIXED_SPACING);
                        *color = codeColor;
                        break;
                    }
                    case MD_BLOCK_P: {
                        font = new BFont(be_plain_font);
                        *color = textColor;
                        break;
                    }
                    case MD_BLOCK_TABLE: {
                        font->SetSpacing(B_FIXED_SPACING);
                        *color = codeColor;
                        break;
                    }
                    default:
                        font = new BFont(be_plain_font);
                        *color = textColor;
                        break;
                }
                break;
            }
            case MARKUP_SPAN: {
                switch (stack_item.markup_type.span_type) {
                    case MD_SPAN_A:
                    case MD_SPAN_WIKILINK: {    // fallthrough
                        font->SetFace(B_UNDERSCORE_FACE);
                        *color = linkColor;
                        break;
                    }
                    case MD_SPAN_IMG: {
                        font->SetSpacing(B_FIXED_SPACING);
                        *color = linkColor;
                        break;
                    }
                    case MD_SPAN_CODE: {
                        font->SetSpacing(B_FIXED_SPACING);
                        *color = codeColor;
                        break;
                    }
                    case MD_SPAN_DEL: {
                        font->SetFace(B_STRIKEOUT_FACE);
                        break;
                    }
                    case MD_SPAN_U: {
                        font->SetFace(B_UNDERSCORE_FACE);
                        break;
                    }
                    case MD_SPAN_STRONG: {
                        font->SetFace(B_BOLD_FACE);
                        break;
                    }
                    case MD_SPAN_EM: {
                        font->SetFace(B_ITALIC_FACE);
                        break;
                    }
                    default:
                        printf("span type %s not handled yet.\n", MarkdownStyler::GetSpanTypeName(stack_item.markup_type.span_type));
                }
                break;
            }
            default: {
                printf("markup type %d (TEXT) not expected here!\n", stack_item.markup_class);
            }
        }
    }
}
