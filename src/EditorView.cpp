/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <ObjectList.h>
#include <stdio.h>

#include "EditorView.h"

EditorView::EditorView() : BView("editor_view", B_WILL_DRAW | B_PULSE_NEEDED) {
    SetLayout(new BGroupLayout(B_VERTICAL, 0));

    fTextView = new BTextView(Bounds(), "textview", Bounds(), B_FOLLOW_ALL, B_FRAME_EVENTS | B_WILL_DRAW);
    fTextView->SetStylable(true);
    fTextView->SetDoesUndo(true);
    fTextView->SetWordWrap(false);
    fTextView->SetFontAndColor(be_plain_font);

    fScrollView = new BScrollView("scrollview", fTextView, B_FOLLOW_ALL, 0, true, true);

    fStatusBar = new StatusBar();

    BLayoutBuilder::Group<>((BGroupLayout*)GetLayout())
        .Add(fScrollView)
        .Add(fStatusBar)
        .End();

    fMarkdownStyler = new MarkdownStyler();
    fMarkdownStyler->Init();
}

EditorView::~EditorView() {
    RemoveSelf();
    delete fStatusBar;
    delete fScrollView;
    delete fMarkdownStyler;
}

void EditorView::SetText(BFile* file, size_t size) {
    fTextView->SetText(file, 0, size);
    MarkupText(0, size);
    fStatusBar->UpdatePosition(0, 0, 0);
}

void EditorView::MarkupText(int32 start, int32 end) {
    if (end == -1) {
        end = fTextView->TextLength();
    }
    int32 size = end - start;
    char text[size + 1];
    BObjectList<text_data>* modes = new BObjectList<text_data>();

    fTextView->GetText(0, size, text);
    fMarkdownStyler->MarkupText(text, size, modes);

    printf("received %d markup text blocks.\n", modes->CountItems());

    text_data* userData;
    int32 offset = 0; // workaround for MD4C anomaly
    text_run_array runs[modes->CountItems()];
    int32 run_index = 0;
    bool isLink;    //test
    bool isCode;    //test

    for (int i = 0; i < modes->CountItems(); i++) {
        userData = modes->ItemAt(i);
        if (userData->offset < size) {
            if (userData->offset > offset) {
                offset = userData->offset;
            } else {
                if (userData->offset > 0) {
                    // verbatim inline blocks sometimes have a relative index ?!
                    offset += userData->offset;
                }
            }
        }
        const char* markupType;
        switch (userData->markup_type) {
            case MARKUP_BLOCK: { markupType = "BLOCK"; break; }
            case MARKUP_SPAN: {
                markupType = "SPAN";
                isLink = false;
                isCode = false;

                if (userData->span_type == MD_SPAN_A) {
                    isLink = true;
                    printf("  got link\n");
                    MD_SPAN_A_DETAIL *detail = (MD_SPAN_A_DETAIL*) userData->detail;
                    /*BString title(detail->title.text, detail->title.size);
                    BString href(detail->href.text, detail->href.size);
                    printf("  got link %s <%s>\n", title.String(), href.String());*/
                } else if (userData->span_type == MD_SPAN_CODE) {
                    isCode = true;
                    printf("  got code block\n");
                }
                break;
            }
            case MARKUP_TEXT: {
                markupType = "TEXT";
                if (isLink) {
                    runs[run_index].count = 1;
                    runs[run_index].runs[0].color = ui_color(B_LINK_TEXT_COLOR);
                    runs[run_index].runs[0].font = be_plain_font;
                    runs[run_index].runs[0].offset = offset;
                    run_index++;
                    // back to normal
                    runs[run_index].count = 1;
                    runs[run_index].runs[0].color = ui_color(B_DOCUMENT_TEXT_COLOR);
                    runs[run_index].runs[0].font = be_plain_font;
                    runs[run_index].runs[0].offset = offset + userData->length - 1;
                    run_index++;
                } else if (isCode) {
                    runs[run_index].count = 1;
                    runs[run_index].runs[0].color = ui_color(B_SHADOW_COLOR);
                    runs[run_index].runs[0].font = be_fixed_font;
                    runs[run_index].runs[0].offset = offset;
                    run_index++;
                    // back to normal
                    runs[run_index].count = 1;
                    runs[run_index].runs[0].color = ui_color(B_DOCUMENT_TEXT_COLOR);
                    runs[run_index].runs[0].font = be_plain_font;
                    runs[run_index].runs[0].offset = offset + userData->length - 1;
                    run_index++;
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
    printf("generated %d runs\n", run_index);
    fTextView->SetRunArray(0, size-1, runs);
}
