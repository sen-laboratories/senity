/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <assert.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <stack>
#include <stdio.h>

#include "EditorTextView.h"

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
    fLinkFont = new BFont(be_plain_font);
    fLinkFont->SetFace(B_UNDERSCORE_FACE);
    fCodeFont = new BFont(be_fixed_font);
    fCodeFont->SetFace(B_CONDENSED_FACE);

    // setup markdown syntax styler
    fMarkdownParser = new MarkdownParser();
    fMarkdownParser->Init();
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
    int32 offset = OffsetAt(where);
    if ((modifiers() & B_COMMAND_KEY) != 0) {
        // highlight block
        int32 begin, end;
        fMarkdownParser->GetMarkupRangeAt(offset, &begin, &end);
        if (begin >= 0 && end > 0) {
            Highlight(begin, end);
            Draw(TextRect());   // bug in TextView not updating highlight correctly
        } else {
            printf("got no boundaries for offset %d!\n", offset);
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
    //fStatusBar->UpdateOutline(outlineItems);
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames) {
    markup_stack *markupStack = fMarkdownParser->GetMarkupRangeAt(offset, NULL, NULL, BLOCK, BEGIN, true, true, true);

    BMessage *outlineMsg = new BMessage('Tout');

    if (markupStack == NULL) {
        printf("no outline at offset %d\n", offset);
        return outlineMsg;
    }

    for (auto item : *markupStack) {
        switch (item->markup_class) {
            case MD_BLOCK_BEGIN: {
                outlineMsg->AddUInt8("block:type", item->markup_type.block_type);
                outlineMsg->AddMessage("block:detail", new BMessage(*item->detail));
                if (withNames) {
                    outlineMsg->AddString("block:name", MarkdownParser::GetBlockTypeName(item->markup_type.block_type));
                }
                break;
            }
            case MD_SPAN_BEGIN: {
                outlineMsg->AddUInt8("span:type", item->markup_type.span_type);
                outlineMsg->AddMessage("span:detail", new BMessage(*item->detail));
                if (withNames) {
                    outlineMsg->AddString("span:name", MarkdownParser::GetSpanTypeName(item->markup_type.span_type));
                }
                break;
            }
            case MD_TEXT: {
                outlineMsg->AddUInt8("text:type", item->markup_type.text_type);
                // MD4C returns no detail for TEXT but let's stay generic here
                outlineMsg->AddMessage("text:detail", new BMessage());
                if (withNames) {
                    outlineMsg->AddString("text:name", MarkdownParser::GetTextTypeName(item->markup_type.text_type));
                }
                break;
            }
            default: {
                // noop
                printf("ignoring markup type %s\n", MarkdownParser::GetMarkupClassName(item->markup_class));
                continue;
            }
        }
    }
    printf("got outline:\n");
    outlineMsg->PrintToStream();

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
        fMarkdownParser->GetMarkupRangeAt(start, &from, &to);
        blockStart = from;
    } else {
        blockStart = 0;
    }
    if (end < TextLength()) {
        fMarkdownParser->GetMarkupRangeAt(end, &from, &to, BLOCK, END);
        blockEnd = to;
    } else {
        blockEnd = TextLength();
    }
    int32 size = blockEnd - blockStart;

    printf("markup text %d - %d\n", blockStart, blockEnd);
    // clear the map section affected by the parser update
    fMarkdownParser->ClearTextInfo(start, end);

    char text[size + 1];
    GetText(blockStart, size, text);

    // perform a partial or complete update of the text map
    fMarkdownParser->Parse(text, size);

    printf("\n*** parsing finished, now styling... ***\n");

    // caching of active styles as detected by block/span ranges
    // push on BLOCK/SPAN_BEGIN, apply on TEXT, pop on BLOCK/SPAN END
    // see https://github.com/mity/md4c/wiki/Embedding-Parser%3A-Calling-MD4C#typical-implementation
    std::stack<text_data*> *blockStyles = new std::stack<text_data*>();
    std::stack<text_data*> *spanStyles = new std::stack<text_data*>();

    // process all text map items in the parsed text
    for (auto info : *(fMarkdownParser->GetMarkupMap())) {
        // process all markup stack items at this map offset
        for (auto stackItem : *info.second) {
            StyleText(stackItem, blockStyles, spanStyles);
        }
    }
    delete blockStyles;
    delete spanStyles;
}

void EditorTextView::StyleText(text_data* markupData,
                                std::stack<text_data*> *blockStyles, std::stack<text_data*> *spanStyles) {
    const char *typeInfo;

    switch (markupData->markup_class) {
        case MD_BLOCK_BEGIN: {
            blockStyles->push(markupData);

            printf("StyleText: got block type %s\n",
                    MarkdownParser::GetBlockTypeName(blockStyles->top()->markup_type.block_type));

            break;
        }
        case MD_BLOCK_END: {
            typeInfo = MarkdownParser::GetBlockTypeName(blockStyles->top()->markup_type.block_type);
            printf("pop block stack element at block_end: %s\n", typeInfo);

            blockStyles->pop();
            break;
        }
        case MD_SPAN_BEGIN: {
            spanStyles->push(markupData);

            printf("StyleText: got span type %s\n",
                    MarkdownParser::GetSpanTypeName(spanStyles->top()->markup_type.span_type));
            break;
        }
        case MD_SPAN_END: {
            typeInfo = MarkdownParser::GetSpanTypeName(spanStyles->top()->markup_type.span_type);
            printf("pop stack stack element at span_end: %s\n", typeInfo);

            spanStyles->pop();
            break;
        }
        case MD_TEXT: {
            int32 start   = markupData->offset;
            int32 end     = start + markupData->length;
            BFont font(be_plain_font);
            rgb_color color = textColor;

            printf("StyleText: got TEXT type %s\n", MarkdownParser::GetTextTypeName(markupData->markup_type.text_type));

            if (! spanStyles->empty()) {
                text_data *spanStyle  = spanStyles->top();
                printf("StyleText @%d - %d: got span style %s\n", start, end,
                    MarkdownParser::GetSpanTypeName(spanStyle->markup_type.span_type));

                SetSpanStyle(spanStyle->markup_type.span_type, spanStyle->detail, &font, &color);
            }
            if (! blockStyles->empty()) {
                text_data *blockStyle = blockStyles->top();
                printf("StyleText @%d - %d: got block style %s\n", start, end,
                    MarkdownParser::GetBlockTypeName(blockStyle->markup_type.block_type));

                SetBlockStyle(blockStyle->markup_type.block_type, blockStyle->detail, &font, &color);
            }

            SetTextStyle(markupData->markup_type.text_type, &font, &color);
            SetFontAndColor(start, end, &font, B_FONT_FAMILY_AND_STYLE, &color);

            typeInfo = MarkdownParser::GetTextTypeName(markupData->markup_type.text_type);
            printf("StyleText @%d - %d: calculated style for class %s, type %s: font face %d, color rgb %d, %d, %d\n",
                start, end,
                MarkdownParser::GetMarkupClassName(markupData->markup_class),
                typeInfo, font.Face(), color.red, color.green, color.blue);

            break;
        }
        default:    // block/span end
            break;
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
