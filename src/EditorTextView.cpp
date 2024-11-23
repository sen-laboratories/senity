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
    if ((modifiers() & B_COMMAND_KEY) != 0) {
        // highlight block
        int32 begin, end;
        GetTextStackForBlockAt(OffsetAt(where), &begin, &end);
        if (begin > 0 && end > 0) {
            Highlight(0, 0);
            Highlight(begin, end);
        }
    }
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
    BMessage* outlineItems = GetOutlineAt(end, true);
    fStatusBar->UpdateOutline(outlineItems);
}

/**
 * get markup detail relevant for the text at the given text offset.
 * collects all text_data blocks FROM the given offet BACK to start of block.
 */
markup_stack* EditorTextView::GetTextStackTo(int32 offset, int32* blockStart) {
    markup_stack *stack = new markup_stack;
    stack->text_stack = new std::vector<text_data>;
    int32 off = offset;
    bool scan = true;

    printf("searching text info at offset %d\n", offset);

    while (off > 0 && scan) {
        off--;
        auto data = fTextInfo->text_map->find(off);
        if (data != fTextInfo->text_map->end()) {
            stack->text_stack->push_back(data->second);
            // todo: advance to previous text data block, now that we are aligned with the map!
            // sadly not possible with C++ iterator here:(
            printf("found text_data class %s\n", MarkdownStyler::GetMarkupClassName(data->second.markup_class));
            if (data->second.markup_class == MD_BLOCK_BEGIN) {
                printf("found BLOCK_BEGIN at %d\n", off);
                if (blockStart != NULL)
                    *blockStart = off;
                scan = false;
            }
        }
    }
    if (stack->text_stack->empty()) {
        printf("no text info found between start and offset %d!\n", offset);
        if (blockStart != NULL)
            *blockStart = -1;
    }
    return stack;
}

markup_stack* EditorTextView::GetTextStackFrom(int32 offset, int32* blockEnd) {
    // search forward for block END to capture the entire block dimensions and collect text_data into stack
    markup_stack *stack = new markup_stack;
    stack->text_stack = new std::vector<text_data>;

    bool scan = true;
    int32 off = offset;

    while (off < TextLength() && scan) {
        off++;
        auto data = fTextInfo->text_map->find(off);
        if (data != fTextInfo->text_map->end()) {
            stack->text_stack->push_back(data->second);
            printf("found text_data class %s\n", MarkdownStyler::GetMarkupClassName(data->second.markup_class));
            if (data->second.markup_class == MD_BLOCK_END) {
                scan = false;
                if (blockEnd != NULL)
                    *blockEnd = off;
                printf("found BLOCK_END at %d\n", off);
            }
        }
    }
    if (off == TextLength()) {
        printf("warning: found no matching BLOCK_END marker in text!");
        if (blockEnd != NULL)
            *blockEnd = -1;
    }
    return stack;
}

markup_stack* EditorTextView::GetTextStackForBlockAt(int32 offset, int32* blockStart, int32* blockEnd) {
    int32 start, end;

    markup_stack *stack = GetTextStackTo(offset, &start);
    GetTextStackFrom(offset, &end);     // we don't need the closing stack, just the end offset if wanted

    if (blockStart != NULL)
        *blockStart = start;
    if (blockEnd != NULL)
        *blockEnd = end;

    return stack;
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames) {
    markup_stack *markupStack = GetTextStackTo(offset);
    BMessage *outlineMsg = new BMessage('Tout');

    for (auto item : *markupStack->text_stack) {
        switch (item.markup_class) {
            case MD_BLOCK_BEGIN: {
                outlineMsg->AddUInt8("block:type", item.markup_type.block_type);
                outlineMsg->AddMessage("block:detail", item.detail != NULL ? item.detail : new BMessage());
                if (withNames) {
                    outlineMsg->AddString("block:name", MarkdownStyler::GetBlockTypeName(item.markup_type.block_type));
                }
                break;
            }
            case MD_SPAN_BEGIN: {
                outlineMsg->AddUInt8("span:type", item.markup_type.span_type);
                outlineMsg->AddMessage("span:detail", item.detail != NULL ? item.detail : new BMessage());
                if (withNames) {
                    outlineMsg->AddString("span:name", MarkdownStyler::GetSpanTypeName(item.markup_type.span_type));
                }
                break;
            }
            case MD_TEXT: {
                outlineMsg->AddUInt8("text:type", item.markup_type.text_type);
                outlineMsg->AddMessage("text:detail", item.detail != NULL ? item.detail : new BMessage());
                if (withNames) {
                    outlineMsg->AddString("text:name", MarkdownStyler::GetTextTypeName(item.markup_type.text_type));
                }
                break;
            }
            default: {
                // noop
                printf("ignoring markup type %s\n", MarkdownStyler::GetMarkupClassName(item.markup_class));
                continue;
            }
        }
    }
    outlineMsg->PrintToStream();
    return outlineMsg;
}

void EditorTextView::ClearTextInfo(int32 start, int32 end) {
    // optimize clear all case
    if (start <= 1 && end >= TextLength()) {
        fTextInfo->text_map->clear();
        return;
    }

    int32 offsetStart, offsetEnd;
    markup_stack *stackStart = GetTextStackTo(start, &offsetStart);
    if (offsetStart < 0) return; // no text info!

    markup_stack *stackEnd = GetTextStackFrom(end, &offsetEnd);
    if (offsetEnd < 0) return;   // bogus, no text info!

    printf("updating map offsets between %d - %d\n", offsetStart, offsetEnd);

    int32 offset = offsetStart; // TODO: how to jump to position and iterate from there??
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

void EditorTextView::StyleText(text_data *textData) {
    BFont font;
    rgb_color color;

    markup_stack *markupStack = GetTextStackTo(textData->offset);
    CalcStyle(markupStack, &font, &color);

    BTextView::SetFontAndColor(textData->offset, textData->offset + textData->length,
                               &font, B_FONT_FAMILY_AND_STYLE, &color);
}

void EditorTextView::CalcStyle(markup_stack* stack, BFont *font, rgb_color *color) {
    for (auto info : *stack->text_stack) {
        if (info.markup_class == MD_BLOCK_BEGIN) {
            GetBlockStyle(info.markup_type.block_type, info.detail, font, color);
        } else if (info.markup_class == MD_SPAN_BEGIN) {
            GetSpanStyle(info.markup_type.span_type, info.detail, font, color);
        }
    }
}

void EditorTextView::GetBlockStyle(MD_BLOCKTYPE blockType, BMessage *detail, BFont *font, rgb_color *color) {
    for (int i = 0; i < detail->CountNames(B_UINT8_TYPE); i++) {
        switch (blockType) {
            case MD_BLOCK_CODE: {
                font = fCodeFont;
                *color = codeColor;
                break;
            }
            case MD_BLOCK_H: {
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
                printf("unhandled block type %s treated as default.\n", MarkdownStyler::GetBlockTypeName(blockType));
                font = new BFont(be_plain_font);
                *color = textColor;
                break;
        }
    }
    printf("got font %d and color brightness %d\n", font->Face(), color->Brightness());
}

void EditorTextView::GetSpanStyle(MD_SPANTYPE spanType, BMessage *detail, BFont *font, rgb_color *color) {
    for (int i = 0; i < detail->CountNames(B_UINT8_TYPE); i++) {
        switch (spanType) {
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
                printf("unhandled block type %s treated as default.\n", MarkdownStyler::GetSpanTypeName(spanType));
                font = new BFont(be_plain_font);
                *color = textColor;
                break;
        }
    }
    printf("got font %d and color brightness %d\n", font->Face(), color->Brightness());
}
