/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <Messenger.h>
#include <stdio.h>

#include "EditorTextView.h"

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
    fCodeFont->SetFace(B_CONDENSED_FACE);

    // setup markdown syntax styler
    fMarkdownStyler = new MarkdownStyler();
    fMarkdownStyler->Init();

    fTextInfo = new text_info;
    fTextInfo->text_map = new std::map<uint, text_data*>;
    fTextInfo->markup_map = new std::map<uint, text_data*>;
}

EditorTextView::~EditorTextView() {
    RemoveSelf();
    fTextInfo->markup_map->clear();
    fTextInfo->text_map->clear();
    delete fTextInfo;
    delete fMarkdownStyler;
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
    MarkupText(start, finish);
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset,
                                const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);
    MarkupText(offset, offset + length);
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
        GetMarkupStackForBlockAt(OffsetAt(where), &begin, &end);
        if (begin > 0 && end > 0) {
            Clear();
            Highlight(begin, end);
            Draw(TextRect());   // bug in TextView not updating highlight correctly
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
 * collects all markup_data blocks FROM the given offet BACK to start of block.
 */
markup_stack* EditorTextView::GetMarkupStackTo(int32 offset, int32* blockStart) {
    markup_stack *stack = new markup_stack;
    stack->markup_stack = new std::vector<text_data*>;
    int32 off = offset;
    bool scan = true;

    printf("searching markup info at offset %d\n", offset);

    while (off >= 0 && scan) {
        auto data = fTextInfo->markup_map->find(off);
        if (data != fTextInfo->markup_map->end()) {
            stack->markup_stack->push_back(data->second);
            printf("found text_data class %s\n", MarkdownStyler::GetMarkupClassName(data->second->markup_class));
            if (data->second->markup_class == MD_BLOCK_BEGIN) {
                if (blockStart != NULL)
                    *blockStart = off;      // text offset (not offset used as map key)
                scan = false;
            }
        }
        off--;
    }
    if (stack->markup_stack->empty()) {
        printf("no text info found between start and offset %d!\n", offset);
        if (blockStart != NULL)
            *blockStart = -1;
    }
    return stack;
}

markup_stack* EditorTextView::GetMarkupStackFrom(int32 offset, int32* blockEnd) {
    // search forward for block END to capture the entire block dimensions and collect text_data into stack
    markup_stack *stack = new markup_stack;
    stack->markup_stack = new std::vector<text_data*>;

    bool scan = true;
    int32 off = offset;

    while (off < TextLength() && scan) {
        auto data = fTextInfo->text_map->find(off);
        if (data != fTextInfo->text_map->end()) {
            stack->markup_stack->push_back(data->second);
            printf("found text_data class %s\n", MarkdownStyler::GetMarkupClassName(data->second->markup_class));
            if (data->second->markup_class == MD_BLOCK_END) {
                scan = false;
                if (blockEnd != NULL)
                    *blockEnd = off;
            }
        }
        off++;
    }
    if (off == TextLength()) {
        printf("warning: found no matching BLOCK_END marker in text!");
        if (blockEnd != NULL)
            *blockEnd = -1;
    }
    return stack;
}

markup_stack* EditorTextView::GetMarkupStackForBlockAt(int32 offset, int32* blockStart, int32* blockEnd) {
    int32 start, end;

    markup_stack *stack = GetMarkupStackTo(offset, &start);
    GetMarkupStackFrom(offset, &end);     // we don't need the closing stack, just the end offset if wanted

    if (blockStart != NULL)
        *blockStart = start;
    if (blockEnd != NULL)
        *blockEnd = end;

    return stack;
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames) {
    markup_stack *markupStack = GetMarkupStackTo(offset);
    BMessage *outlineMsg = new BMessage('Tout');

    for (auto item : *markupStack->markup_stack) {
        switch (item->markup_class) {
            case MD_BLOCK_BEGIN: {
                outlineMsg->AddUInt8("block:type", item->markup_type.block_type);
                outlineMsg->AddMessage("block:detail", new BMessage(*item->detail));
                if (withNames) {
                    outlineMsg->AddString("block:name", MarkdownStyler::GetBlockTypeName(item->markup_type.block_type));
                }
                break;
            }
            case MD_SPAN_BEGIN: {
                outlineMsg->AddUInt8("span:type", item->markup_type.span_type);
                outlineMsg->AddMessage("span:detail", new BMessage(*item->detail));
                if (withNames) {
                    outlineMsg->AddString("span:name", MarkdownStyler::GetSpanTypeName(item->markup_type.span_type));
                }
                break;
            }
            default: {
                // noop
                printf("ignoring markup type %s\n", MarkdownStyler::GetMarkupClassName(item->markup_class));
                continue;
            }
        }
        // check if there is a text at that markup position
        auto textItem = fTextInfo->text_map->find(item->offset);
        if (textItem != fTextInfo->text_map->cend()) {
            outlineMsg->AddUInt8("text:type", textItem->second->markup_type.text_type);
            // MD4C returns no detail for TEXT but let's stay generic here
            outlineMsg->AddMessage("text:detail", textItem->second->detail);
            if (withNames) {
                outlineMsg->AddString("text:name", MarkdownStyler::GetTextTypeName(textItem->second->markup_type.text_type));
            }
            break;
        }
    }
    printf("got outline:\n");
    outlineMsg->PrintToStream();

    return outlineMsg;
}

void EditorTextView::ClearTextInfo(int32 start, int32 end) {
    // noop case
    if (TextLength() == 0)
        return;
    // optimize clear all case
    if (start <= 1 && end >= TextLength()) {
        fTextInfo->markup_map->clear();
        fTextInfo->text_map->clear();
        return;
    }

    int32 offsetStart, offsetEnd;
    GetMarkupStackTo(start, &offsetStart);
    if (offsetStart < 0) return; // no text info!

    GetMarkupStackFrom(end, &offsetEnd);
    if (offsetEnd < 0) return;   // bogus, no text info!

    printf("updating map offsets between %d - %d\n", offsetStart, offsetEnd);
    auto infoAtOffsetIter = std::next(fTextInfo->markup_map->begin(), offsetStart);

    while (infoAtOffsetIter != fTextInfo->markup_map->end()) {
        int32 offset = infoAtOffsetIter->first;
        if (offset > offsetEnd) {
            printf("index @%d > end %d, done.\n", offset, offsetEnd);
            break;
        }
        printf("removing outdated markup info @%d\n", offset);
        fTextInfo->markup_map->erase(offset);

        auto textItem = fTextInfo->text_map->find(offset);
        if (textItem != fTextInfo->text_map->cend()) {
            printf("removing outdated text info @%d\n", offset);
            fTextInfo->text_map->erase(offset);
        }
        infoAtOffsetIter++;
    }
}

// interaction with MarkupStyler - should become its own class later
void EditorTextView::MarkupText(int32 start, int32 end) {
    if (end == -1) {
        end = TextLength();
    }
    if (TextLength() == 0) {
        return;
    }
    // extend to block offsets so markup can be determined and styled correctly
    int32 blockStart, blockEnd;
    if (fTextInfo->text_map->empty()) {
        blockStart = 0;
        blockEnd = TextLength()-1;
    } else {
        if (start > 0)
            GetMarkupStackTo(start, &blockStart);
        else
            blockStart = 0;
        if (end < TextLength())
            GetMarkupStackFrom(end, &blockEnd);
        else
            blockEnd = TextLength();
    }
    int32 size = blockEnd - blockStart;
    printf("markup text %d - %d\n", blockStart, blockEnd);
    // clear the map section affected by the parser update
    // todo: this method determines block ranges again, optimize!
    ClearTextInfo(start, end);

    char text[size + 1];
    GetText(blockStart, size, text);

    // perform a partial or complete update of the text map
    fMarkdownStyler->MarkupText(text, size, fTextInfo);

    printf("\n*** received %zu markup and %zu text blocks, now styling... ***\n",
        fTextInfo->markup_map->size(),
        fTextInfo->text_map->size());

    BFont       font;
    rgb_color   color(textColor);

    for (auto info : *fTextInfo->markup_map) {
        // &info.first is the offset used as map key and for editor lookup on select/position
        // for easier processing, it is also contained in &info.second and can be ignored here.
        StyleText(info.second, &font, &color);
    }
}

void EditorTextView::StyleText(text_data* markupData, BFont *font, rgb_color *color) {
    const char *typeInfo;

    switch (markupData->markup_class) {
        case MD_BLOCK_BEGIN: {
            SetBlockStyle(markupData->markup_type.block_type, markupData->detail, font, color);
            typeInfo = MarkdownStyler::GetBlockTypeName(markupData->markup_type.block_type);
            break;
        }
        case MD_SPAN_BEGIN: {
            SetSpanStyle(markupData->markup_type.span_type, markupData->detail, font, color);
            typeInfo = MarkdownStyler::GetSpanTypeName(markupData->markup_type.span_type);
            break;
        }
        // todo handle nested blocks and spans as described in
        // https://github.com/mity/md4c/wiki/Embedding-Parser%3A-Calling-MD4C#typical-implementation
        default:    // block/span end
            //*font  = *be_plain_font;
            //*color = textColor;
            break;
    }

    // check for text style at markup position
    auto textItemIter = fTextInfo->text_map->find(markupData->offset);
    if (textItemIter != fTextInfo->text_map->cend()) {
        auto textItem = textItemIter->second;
        SetTextStyle(textItem->markup_type.text_type, font, color);
        typeInfo      = MarkdownStyler::GetTextTypeName(textItem->markup_type.text_type);

        int32 start   = textItem->offset;
        int32 end     = start + textItem->length;
        SetFontAndColor(start, end, font, B_FONT_FAMILY_AND_STYLE, color);
        // debug
        printf("StyleText @%d - %d: calculated style for class %s, type %s: font face %d, color rgb %d, %d, %d\n",
            start, end,
            MarkdownStyler::GetMarkupClassName(markupData->markup_class),
            typeInfo, font->Face(), color->red, color->green, color->blue);
    } else {
        printf("StyleText @%d: no text info found!\n", markupData->offset);
    }
}

void EditorTextView::SetBlockStyle(MD_BLOCKTYPE blockType, BMessage *detail, BFont *font, rgb_color *color) {
    switch (blockType) {
        case MD_BLOCK_CODE: {
            *font = *fCodeFont;
            *color = codeColor;
            break;
        }
        case MD_BLOCK_H: {
            uint8 level;
            if (detail->FindUInt8("level", &level) == B_OK) {
                float headerSizeFac = (7 - level) / 3.2;                // max 6 levels in markdown
                font->SetSize(be_plain_font->Size() * headerSizeFac);   // H1 = 2*normal size
                font->SetFace(B_HEAVY_FACE);
            }
            *color = codeColor;
            break;
        }
        case MD_BLOCK_QUOTE: {
            font->SetFace(B_ITALIC_FACE);
            *color = codeColor;
            break;
        }
        case MD_BLOCK_HR: {
            font->SetFace(B_LIGHT_FACE);
            *color = codeColor;
            break;
        }
        case MD_BLOCK_HTML: {
            font->SetSpacing(B_FIXED_SPACING);
            *color = codeColor;
            break;
        }
        case MD_BLOCK_P: {
            *font = *be_plain_font;
            *color = textColor;
            break;
        }
        case MD_BLOCK_TABLE: {
            font->SetSpacing(B_FIXED_SPACING);
            *color = codeColor;
            break;
        }
        default:
            break;
    }
}

void EditorTextView::SetSpanStyle(MD_SPANTYPE spanType, BMessage *detail, BFont *font, rgb_color *color) {
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
            break;
    }
}

void EditorTextView::SetTextStyle(MD_TEXTTYPE textType, BFont *font, rgb_color *color) {
    switch (textType) {
        case MD_TEXT_CODE:              // same for now
        case MD_TEXT_HTML: {
            font->SetSpacing(B_FIXED_SPACING);
            *color = codeColor;
            break;
        }
        default:
            // don't override block and span styles here! just keep it as is
            break;
    }
}
