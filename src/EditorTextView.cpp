/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <assert.h>
#include <Messenger.h>
#include <Region.h>
#include <ScrollView.h>
#include <stdio.h>

#include "EditorTextView.h"

using namespace std;

EditorTextView::EditorTextView(StatusBar *statusBar, BHandler *editorHandler)
: BTextView("editor_text_view")
{
	SetViewUIColor(B_DOCUMENT_BACKGROUND_COLOR);
	SetLowUIColor(ViewUIColor());

    SetStylable(true);
    SetDoesUndo(true);
    SetWordWrap(false);
    SetFontAndColor(be_plain_font);

    fStatusBar = statusBar;
    fMessenger = new BMessenger(editorHandler);

    // setup fonts
    fTextFont = new BFont(be_plain_font);
    fLinkFont = new BFont(be_plain_font);
    fCodeFont = new BFont(be_fixed_font);

    // setup markdown syntax styler
    fMarkdownParser = new MarkdownParser();
    fMarkdownParser->Init();

    fTextHighlights = new map<int32, text_highlight*>();
}

EditorTextView::~EditorTextView() {
    RemoveSelf();
    delete fMarkdownParser;
    delete fMessenger;
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

// hook methods
void EditorTextView::DeleteText(int32 start, int32 finish) {
    BTextView::DeleteText(start, finish);
    MarkupText(start, finish);
    UpdateStatus();
}

void EditorTextView::InsertText(const char* text, int32 length, int32 offset,
                                const text_run_array* runs)
{
    BTextView::InsertText(text, length, offset, runs);
    MarkupText(offset, offset + length);
    UpdateStatus();
}

void EditorTextView::KeyDown(const char* bytes, int32 numBytes) {
    BTextView::KeyDown(bytes, numBytes);

    UpdateStatus();
}

void EditorTextView::MouseDown(BPoint where) {
    BTextView::MouseDown(where);
    UpdateStatus();
    if (TextLength() == 0) return;

    int32 offset = OffsetAt(where);
    if ((modifiers() & B_COMMAND_KEY) != 0) {
        // highlight block
        int32 begin, end;
        fMarkdownParser->GetMarkupBoundariesAt(offset, &begin, &end, BLOCK, BOTH);
        if (begin >= 0 && end > 0) {
            printf("highlighting text from %d - %d\n", begin, end);
            Highlight(begin, end, NULL, &linkColor);
        } else {
            printf("got no boundaries for offset %d!\n", offset);
        }
    } else {
        auto data = fMarkdownParser->GetMarkupStackAt(offset);
        BString stack;
        for (auto item : *data) {
            stack << "@" << item->offset << ": " <<
            MarkdownParser::GetMarkupClassName(item->markup_class) << " [" <<
            (item->markup_class == MD_BLOCK_BEGIN || item->markup_class == MD_BLOCK_END ?
                MarkdownParser::GetBlockTypeName(item->markup_type.block_type) :
                    (item->markup_class == MD_SPAN_BEGIN || item->markup_class == MD_SPAN_END ?
                        MarkdownParser::GetSpanTypeName(item->markup_type.span_type) :
                            MarkdownParser::GetTextTypeName(item->markup_type.text_type)    // must be TEXT
                    )
            ) <<
            "]";
            if (item != *data->end())
                stack << " | ";
        }
        printf("markup stack at offset %d (%zu items): %s\n", offset, data->size(), stack.String());
    }
}

void EditorTextView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) {
    BTextView::MouseMoved(where, code, dragMessage);
}

void EditorTextView::Draw(BRect updateRect) {
    BTextView::Draw(updateRect);

    // redraw text highlights if any inside updateRect

    // TODO: optimize later via smart map lookup
    // int32 updateOffset = OffsetAt(updateRect.LeftTop());
    // auto highlight = fTextHighlights->find(updateOffset); //todo: get nearest offsets to updateRect boundaries

    printf("redrawing text highlights, scanning %zu highlights...\n", fTextHighlights->size());

    for (auto highlight : *fTextHighlights) {
        auto textHighlight = highlight.second;
        if (textHighlight->region->Intersects(updateRect)) {
            printf("redraw highlight at offset %d...\n", textHighlight->startOffset);
            Highlight(
                textHighlight->startOffset,
                textHighlight->endOffset,
                textHighlight->fgColor,
                textHighlight->bgColor);
        }
    }
}

void
EditorTextView::Highlight(int32 startOffset, int32 endOffset, const rgb_color *fgColor, const rgb_color *bgColor)
{
	// pin offsets at reasonable values
	if (startOffset < 0)
		startOffset = 0;
	else if (startOffset > TextLength())
		startOffset = TextLength() - 1;
	if (endOffset < 0)
		endOffset = 0;
	else if (endOffset > TextLength())
		endOffset = TextLength() - 1;

	if (startOffset >= endOffset)
		return;

    printf("Highlight: from %d - %d\n", startOffset, endOffset);
    BPoint textLoc = PointAt(startOffset);

	BRegion selRegion;
	GetTextRegion(startOffset, endOffset, &selRegion);

    auto savedHighlight = fTextHighlights->find(startOffset);
    if (savedHighlight == fTextHighlights->end()) {
        printf("Highlight: store new highlight in map...\n");

        // add to saved highlights for redraw
        text_highlight *highlight = new text_highlight;
        highlight->startOffset = startOffset;
        highlight->endOffset   = endOffset;
        highlight->region      = new BRegion(selRegion);
        highlight->fgColor     = fgColor;
        highlight->bgColor     = bgColor;

        fTextHighlights->insert({startOffset, highlight});
    }   // else it's just a redraw (TODO: support updating highlights)

    // provide a UTF-8 aware char buffer for GetText()
    int32 len = endOffset - startOffset + 1;
    BString textStr("", len);
    char* text = textStr.LockBuffer(len);

    GetText(startOffset, len, text);
    textStr.UnlockBuffer();

	SetDrawingMode(B_OP_BLEND);
    rgb_color hiCol = HighColor();
    rgb_color loCol = LowColor();

    // draw background label
    SetLowColor(bgColor != NULL ? *bgColor : LowColor());
	FillRegion(&selRegion, B_SOLID_LOW);

    // draw foreground label text
    SetHighColor(fgColor != NULL ? *fgColor : textColor);
	DrawString(text, textLoc);

    SetLowColor(loCol);
    SetHighColor(hiCol);
	SetDrawingMode(B_OP_COPY);
}

void EditorTextView::UpdateStatus() {
    int32 start, end, line;
    line = CurrentLine();
    GetSelection(&start, &end);

    fStatusBar->UpdatePosition(end, CurrentLine(), end - OffsetAt(line));
    fStatusBar->UpdateSelection(start, end);

    // update outline in status from block / span info contained in text info stack
    BMessage* outline = GetOutlineAt(start, true);
    fStatusBar->UpdateOutline(outline);
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames) {
    int32 blockOffset;
    outline_map* outlineMap = fMarkdownParser->GetOutlineAt(offset);

    BMessage *outlineMsg = new BMessage('Tout');

    if (outlineMap == NULL || outlineMap->empty()) {
        printf("no outline at offset %d\n", offset);
        return outlineMsg;
    }

    printf("GetOutline: got outline map at offset %d with %zu elements.\n", offset, outlineMap->size());

    for (auto item : *outlineMap) {
        outlineMsg->AddPointer(item.first, item.second);
    }
    printf("got outline:\n");
    outlineMsg->PrintToStream();

    return outlineMsg;
}

BMessage* EditorTextView::GetDocumentOutline(bool withDetails, bool withNames) {
    BMessage *outlineMsg = new BMessage('Tout');
    bool addOffset = false;

    for (auto mapItem : *(fMarkdownParser->GetMarkupMap())) {
        for (auto item : *mapItem.second) {
            switch (item->markup_class) {
                case MD_BLOCK_BEGIN: {
                    outlineMsg->AddUInt8("block:type", item->markup_type.block_type);
                    if (withDetails) {
                        outlineMsg->AddMessage("block:detail", new BMessage(*item->detail));
                    }
                    if (withNames) {
                        BString blockName = MarkdownParser::GetBlockTypeName(item->markup_type.block_type);
                        // for Header, add level like in HTML (H1...H6)
                        if (item->markup_type.block_type == MD_BLOCK_H) {
                            uint8 level = item->detail->GetUInt8("level", 0);
                            blockName << level;
                        }
                        outlineMsg->AddString("block:name", blockName.String());
                    }
                    addOffset = true;
                    break;
                }
                case MD_SPAN_BEGIN: {
                    outlineMsg->AddUInt8("span:type", item->markup_type.span_type);
                    if (withDetails) {
                        outlineMsg->AddMessage("span:detail", new BMessage(*item->detail));
                    }
                    if (withNames) {
                        outlineMsg->AddString("span:name", MarkdownParser::GetSpanTypeName(item->markup_type.span_type));
                    }
                    addOffset = true;
                    break;
                }
                case MD_TEXT: {
                    outlineMsg->AddUInt8("text:type", item->markup_type.text_type);
                    // MD4C returns no detail for TEXT but let's stay generic here
                    if (withDetails) {
                        outlineMsg->AddMessage("text:detail", new BMessage());
                    }
                    if (withNames) {
                        outlineMsg->AddString("text:name", MarkdownParser::GetTextTypeName(item->markup_type.text_type));
                    }
                    addOffset = true;
                    break;
                }
                default: {
                    // noop
                    printf("ignoring markup type %s\n", MarkdownParser::GetMarkupClassName(item->markup_class));
                    break;
                }
            }
            if (addOffset) {
                // reference to location inside text
                outlineMsg->AddInt32("offset", item->offset);
                addOffset = false;
            }
        }
    }
    return outlineMsg;
}

// interaction with MarkupStyler - should become its own class later
void EditorTextView::MarkupText(int32 start, int32 end) {
    if (TextLength() == 0) {
        return;
    }
    int32 blockStart, blockEnd;
    // extend to block offsets for start and end so markup can be determined and styled correctly
    // since we have 2 possibly overlapping boundaries for block around start and end offset from edit,
    // we need to use temp vars here and just take the first start boundary and the last end boundary.
    int32 from, to;
    if (start > 0) {
        fMarkdownParser->GetMarkupBoundariesAt(start, &from, &to);
        blockStart = from;
    } else {
        blockStart = 0;
    }
    if (end < TextLength()) {
        fMarkdownParser->GetMarkupBoundariesAt(end, &from, &to, BLOCK, END);
        blockEnd = to;
    } else {
        blockEnd = TextLength();
    }
    int32 size = blockEnd - blockStart;

    printf("markup text %d - %d\n", blockStart, blockEnd);
    // clear the map section affected by the parser update
    fMarkdownParser->ClearTextInfo(start, end);

    BString textStr("", size);
    char* text = textStr.LockBuffer(size);
    GetText(blockStart, size, text);
    textStr.UnlockBuffer();

    // perform a partial or complete update of the text map
    fMarkdownParser->Parse(text, size);

    printf("\n*** parsing finished, now styling... ***\n");

    // we need to use a stack for caching the last active block/span style to return to on BLOCK_END or SPAN_END,
    // since we cannot simply "undo" the last style, as they might be stacked inside each other.
    // see https://github.com/mity/md4c/wiki/Embedding-Parser%3A-Calling-MD4C#typical-implementation
    BFont font(be_fixed_font);
    rgb_color color(textColor);
    stack<text_run> styleStack;

    // process all text map items in the parsed text
    for (auto info : *(fMarkdownParser->GetMarkupMap())) {
        auto item = *info.second->begin();
        // reset for text-only parts
        if (info.second->size() == 1 && item->markup_class == MD_TEXT) {
            font = *fTextFont;
            color = textColor;
        }
        // process all markup stack items at this map offset
        for (auto stackItem : *info.second) {
            StyleText(stackItem, &styleStack, &font, &color);
        }
    }

    printf("DocumentOutline:\n");
    GetDocumentOutline(true)->PrintToStream();
}

void EditorTextView::StyleText(text_data* markupData,
                               stack<text_run> *styleStack,
                               BFont* font, rgb_color* color) {
    const char *typeInfo;

    switch (markupData->markup_class) {
        case MD_BLOCK_BEGIN:
        case MD_SPAN_BEGIN: {
            if (markupData->markup_class == MD_BLOCK_BEGIN) {
                SetBlockStyle(markupData, font, color);
            }
            else {
                SetSpanStyle(markupData, font, color);
            }
            text_run run;
            run.color = *color;
            run.font  = *font;
            styleStack->push(text_run(run));
            printf("> pushing %s style -> %zu runs.\n", MarkdownParser::GetMarkupClassName(markupData->markup_class),
                styleStack->size());
            break;
        }
        case MD_BLOCK_END:
        case MD_SPAN_END: {
            if (!styleStack->empty()) {
                text_run run = styleStack->top();
                *font = run.font;
                *color = run.color;

                styleStack->pop();
            }
            printf("< popping %s style -> %zu runs.\n", MarkdownParser::GetMarkupClassName(markupData->markup_class),
                styleStack->size());
            break;
        }
        case MD_TEXT: {         // here the styles set before are actually applied to rendered text
            int32 start   = markupData->offset;
            int32 end     = start + markupData->length;

            // reset for text-only parts
            if (styleStack->size() == 0) {
                *font = *fTextFont;
                *color = textColor;
            }

            SetTextStyle(markupData, font, color);
            SetFontAndColor(start, end, font, B_FONT_ALL, color);

            typeInfo = MarkdownParser::GetTextTypeName(markupData->markup_type.text_type);
            printf("StyleText @%d - %d: applied style for class %s and type %s\n",
                start, end, MarkdownParser::GetMarkupClassName(markupData->markup_class), typeInfo);

            break;
        }
        default:
            break;
    }
}

void EditorTextView::SetBlockStyle(text_data* markupInfo, BFont *font, rgb_color *color) {
    MD_BLOCKTYPE blockType = markupInfo->markup_type.block_type;
    const char* blockTypeName = MarkdownParser::GetBlockTypeName(blockType);

    printf("> SetBlockStyle for %s\n", blockTypeName);

    switch (blockType) {
        case MD_BLOCK_CODE: {
            *font = *fCodeFont;
            *color = codeColor;
            break;
        }
        case MD_BLOCK_H: {
            *font = fTextFont;
            uint8 level;
            BMessage *detail = markupInfo->detail;
            if (detail == NULL) {
                printf("    bogus markup, no detail found for H block!\n");
            } else if (detail->FindUInt8("level", &level) == B_OK) {
                float headerSizeFac = (7 - level) / 3.2;       // max 6 levels in markdown
                font->SetSize(font->Size() * headerSizeFac);   // H1 = 2*normal size
                font->SetFace(B_HEAVY_FACE);
            }
            *color = headerColor;
            break;
        }
        case MD_BLOCK_QUOTE: {
            *font = fTextFont;
            font->SetFace(font->Face() | B_ITALIC_FACE);
            *color = codeColor;
            break;
        }
        case MD_BLOCK_HR: {
            *font = *fTextFont;
            *color = headerColor;
            font->SetFace(font->Face() | B_LIGHT_FACE);
            break;
        }
        case MD_BLOCK_HTML: {
            *font = fCodeFont;
            *color = codeColor;
            break;
        }
        case MD_BLOCK_UL: {
            *font = fTextFont;
            *color = textColor;
            break;
        }
        case MD_BLOCK_OL: {
            *font = fTextFont;
            *color = textColor;
            break;
        }
        case MD_BLOCK_LI: {
            *font = fTextFont;
            *color = linkColor;
            break;
        }
        case MD_BLOCK_P: {
            *font = fTextFont;
            *color = textColor;
            break;
        }
        case MD_BLOCK_TABLE: {
            *font = fCodeFont;
            *color = codeColor;
            break;
        }
        default:
            break;
    }
}

void EditorTextView::SetSpanStyle(text_data* markupInfo, BFont *font, rgb_color *color) {
    MD_SPANTYPE spanType = markupInfo->markup_type.span_type;
    const char* spanTypeName = MarkdownParser::GetSpanTypeName(spanType);

    printf("> SetSpanStyle for %s\n", spanTypeName);

    switch (spanType) {
        case MD_SPAN_A:
        case MD_SPAN_WIKILINK: {    // fallthrough
            font->SetFace(font->Face() | B_UNDERSCORE_FACE);
            *color = linkColor;
            break;
        }
        case MD_SPAN_IMG: {
            font->SetFace(font->Face() | B_REGULAR_FACE);
            *color = linkColor;
            break;
        }
        case MD_SPAN_CODE: {
            font->SetFace(font->Face() | B_REGULAR_FACE);
            *color = codeColor;
            break;
        }
        case MD_SPAN_DEL: {
            font->SetFace(font->Face() | B_STRIKEOUT_FACE);
            *color = codeColor;
            break;
        }
        case MD_SPAN_U: {
            font->SetFace(font->Face() | B_UNDERSCORE_FACE);
            *color = textColor;
            break;
        }
        case MD_SPAN_STRONG: {
            font->SetFace(font->Face() | B_BOLD_FACE);
            *color = textColor;
            break;
        }
        case MD_SPAN_EM: {
            font->SetFace(font->Face() | B_ITALIC_FACE);
            *color = textColor;
            break;
        }
        default:
            font->SetFace(font->Face() | B_REGULAR_FACE);
            *color = textColor;
            break;
    }
}

void EditorTextView::SetTextStyle(text_data* markupInfo, BFont *font, rgb_color *color) {
    MD_TEXTTYPE textType = markupInfo->markup_type.text_type;

    switch (textType) {
        case MD_TEXT_CODE: {
            font->SetSpacing(B_FIXED_SPACING);
            *color = codeColor;
            break;
        }
        case MD_TEXT_HTML: {
            font->SetSpacing(B_FIXED_SPACING);
            *color = linkColor;
            break;
        }
        case MD_TEXT_NORMAL: {
            // use span/block style here!
            break;
        }
        default:
            *font = *fTextFont;
            *color = textColor;
            break;
    }
}
