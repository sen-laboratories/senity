/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <assert.h>
#include <Messenger.h>
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
    fTextFont = new BFont(be_fixed_font);     // same as code font for now
    fLinkFont = new BFont(be_plain_font);
    fLinkFont->SetFace(B_UNDERSCORE_FACE);
    fCodeFont = new BFont(be_fixed_font);

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
    UpdateStatus();
    if (TextLength() == 0) return;

    int32 offset = OffsetAt(where);
    if ((modifiers() & B_COMMAND_KEY) != 0) {
        // highlight block
        int32 begin, end;
        fMarkdownParser->GetMarkupBoundariesAt(offset, &begin, &end);
        if (begin >= 0 && end > 0) {
            printf("highlighting text from %d - %d\n", begin, end);
            Highlight(begin, end);
            Invalidate();   // bug in TextView not updating highlight correctly
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

void EditorTextView::UpdateStatus() {
    int32 start, end, line;
    GetSelection(&start, &end);
    line = CurrentLine();

    fStatusBar->UpdatePosition(end, CurrentLine(), end - OffsetAt(line));
    fStatusBar->UpdateSelection(start, end);

    // update outline in status from block / span info contained in text info stack
    BMessage* outline = GetOutlineAt(start, true);
    fStatusBar->UpdateOutline(outline);
}

BMessage* EditorTextView::GetOutlineAt(int32 offset, bool withNames) {
    int32 blockOffset;
    markup_stack *markupStack = fMarkdownParser->GetOutlineAt(offset);

    BMessage *outlineMsg = new BMessage('Tout');

    if (markupStack == NULL || markupStack->empty()) {
        printf("no outline at offset %d\n", offset);
        return outlineMsg;
    }

    printf("GetOutline: got markup stack at offset %d with %zu elements.\n", offset, markupStack->size());

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

    char text[size + 1];
    GetText(blockStart, size, text);

    // perform a partial or complete update of the text map
    fMarkdownParser->Parse(text, size);

    printf("\n*** parsing finished, now styling... ***\n");

    // we don't need to use a stack or similar since we only apply a partial style
    // as per block/span type and detail upon BLOCK/SPAN BEGIN, and later "unapply"
    // that partial style upon BLOCK/SPAN END.
    // see https://github.com/mity/md4c/wiki/Embedding-Parser%3A-Calling-MD4C#typical-implementation
    BFont font(be_fixed_font);
    rgb_color color(textColor);

    // process all text map items in the parsed text
    for (auto info : *(fMarkdownParser->GetMarkupMap())) {
        // process all markup stack items at this map offset
        for (auto stackItem : *info.second) {
            StyleText(stackItem, &font, &color);
        }
    }
}

void EditorTextView::StyleText(text_data* markupData, BFont* font, rgb_color* color) {
    const char *typeInfo;

    switch (markupData->markup_class) {
        case MD_BLOCK_BEGIN:    // fallthrough, BEGIN/END decision handled in StyleBlock
        case MD_BLOCK_END: {
            SetBlockStyle(markupData, font, color);
            break;
        }
        case MD_SPAN_BEGIN:     // see above comment
        case MD_SPAN_END: {
            SetSpanStyle(markupData, font, color);
            break;
        }
        case MD_TEXT: {         // here the styles set before are actually applied to rendered text
            int32 start   = markupData->offset;
            int32 end     = start + markupData->length;

            SetTextStyle(markupData->markup_type.text_type, font, color);
            SetFontAndColor(start, end, font, B_FONT_ALL, color);

            typeInfo = MarkdownParser::GetTextTypeName(markupData->markup_type.text_type);
            printf("StyleText @%d - %d: applied style for class %s and type %s\n",
                start, end, MarkdownParser::GetMarkupClassName(markupData->markup_class), typeInfo);

            break;
        }
        default:    // doc/para not handled here because not relevant for styling
            break;
    }
}

void EditorTextView::SetBlockStyle(text_data* markupInfo, BFont *font, rgb_color *color) {
    MD_BLOCKTYPE blockType = markupInfo->markup_type.block_type;
    bool apply = (markupInfo->markup_class == MD_BLOCK_BEGIN);
    const char* blockTypeName = MarkdownParser::GetBlockTypeName(blockType);

    printf("    SetBlockStyle %s: %s\n", blockTypeName, apply ? "ON" : "OFF");

    switch (blockType) {
        case MD_BLOCK_CODE: {
            if (apply) {
                *font = *fCodeFont;
                *color = codeColor;
            } else {
                *font = *fTextFont;
                *color = textColor;
            }
            break;
        }
        case MD_BLOCK_H: {
            if (apply) {
                *font = fTextFont;
                uint8 level;
                BMessage *detail = markupInfo->detail;
                if (detail == NULL) {
                    printf("    internal error, no detail found for H block!\n");
                    break;
                }
                if (detail->FindUInt8("level", &level) == B_OK) {
                    float headerSizeFac = (7 - level) / 3.2;       // max 6 levels in markdown
                    font->SetSize(font->Size() * headerSizeFac);   // H1 = 2*normal size
                    font->SetFace(B_HEAVY_FACE);
                }
                *color = headerColor;
            } else {
                *color = textColor;
            }
            break;
        }
        case MD_BLOCK_QUOTE: {
            if (apply) {
                font->SetFace(font->Face() | B_ITALIC_FACE);
                *color = codeColor;
            } else {
                font->SetFace(font->Face() &~ B_ITALIC_FACE);
                *color = textColor;
            }
            break;
        }
        case MD_BLOCK_HR: {
            if (apply) {
                *font = *fTextFont;
                *color = headerColor;
                font->SetFace(font->Face() | B_LIGHT_FACE);
            }
            else {
                *color = textColor;
                font->SetFace(font->Face() &~ B_LIGHT_FACE);
            }
            break;
        }
        case MD_BLOCK_HTML: {
            if (apply) {
                *font = fCodeFont;
                *color = codeColor;
            } else {
                *font = fTextFont;
                *color = textColor;
            }
            break;
        }
        case MD_BLOCK_UL: {
            if (apply) {
                *color = linkColor;
            } else {
                *color = textColor;
            }
            break;
        }
        case MD_BLOCK_OL: {
            if (apply) {
                *color = linkColor;
            } else {
                *color = textColor;
            }
            break;
        }
        case MD_BLOCK_LI: {
            *font = fTextFont;
            *color = textColor;
            break;
        }
        case MD_BLOCK_P: {
            *font = fTextFont;
            *color = textColor;
            break;
        }
        case MD_BLOCK_TABLE: {
            if (apply) {
                *font = fCodeFont;
                *color = codeColor;
            } else {
                *font = fTextFont;
                *color = textColor;
            }
            break;
        }
        default:
            break;
    }
}

void EditorTextView::SetSpanStyle(text_data* markupInfo, BFont *font, rgb_color *color) {
    MD_SPANTYPE spanType = markupInfo->markup_type.span_type;
    bool apply = (markupInfo->markup_class == MD_SPAN_BEGIN);
    const char* spanTypeName = MarkdownParser::GetSpanTypeName(spanType);

    printf("    SetSpanStyle %s: %s\n", spanTypeName, apply ? "ON" : "OFF");

    switch (spanType) {
        case MD_SPAN_A:
        case MD_SPAN_WIKILINK: {    // fallthrough
            if (apply) {
                font->SetFace(font->Face() | B_UNDERSCORE_FACE);
                *color = linkColor;
            } else {
                font->SetFace(font->Face() &~ B_UNDERSCORE_FACE);
                *color = textColor;
            }
            break;
        }
        case MD_SPAN_IMG: {
            if (apply) {
                *color = linkColor;
            } else {
                *color = textColor;
            }
            break;
        }
        case MD_SPAN_CODE: {
            if (apply) {
                *color = codeColor;
            } else {
                *color = textColor;
            }
            break;
        }
        case MD_SPAN_DEL: {
            if (apply) {
                font->SetFace(font->Face() | B_STRIKEOUT_FACE);
            } else {
                font->SetFace(font->Face() &~ B_STRIKEOUT_FACE);
            }
            break;
        }
        case MD_SPAN_U: {
            if (apply) {
                font->SetFace(font->Face() | B_UNDERSCORE_FACE);
            } else {
                font->SetFace(font->Face() &~ B_UNDERSCORE_FACE);
            }
            break;
        }
        case MD_SPAN_STRONG: {
            if (apply) {
                font->SetFace(font->Face() | B_BOLD_FACE);
            } else {
                font->SetFace(font->Face() &~ B_BOLD_FACE);
            }
            break;
        }
        case MD_SPAN_EM: {
            if (apply) {
                font->SetFace(font->Face() | B_ITALIC_FACE);
            } else {
                font->SetFace(font->Face() &~ B_ITALIC_FACE);
            }
            break;
        }
        default:
            break;
    }
}

void EditorTextView::SetTextStyle(MD_TEXTTYPE textType, BFont *font, rgb_color *color) {
    switch (textType) {
        case MD_TEXT_CODE: {
            printf("set text style CODE.\n");
            font->SetSpacing(B_FIXED_SPACING);
            *color = codeColor;
            break;
        }
        case MD_TEXT_HTML: {
            printf("set text style HTML.\n");
            font->SetSpacing(B_FIXED_SPACING);
            *color = linkColor;
            break;
        }
        default:
            // don't override block and span styles here! just keep it as is
            break;
    }
}
