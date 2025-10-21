/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <ObjectList.h>
#include <Screen.h>
#include <stdio.h>

#include "EditorView.h"
#include "Messages.h"

EditorView::EditorView() : BView("editor_view", B_WILL_DRAW | B_PULSE_NEEDED | B_FRAME_EVENTS)
{
    fColorDefs  = new ColorDefs();
    fStatusBar  = new StatusBar();
    fTextView   = new EditorTextView(fStatusBar, this);
    fScrollView = new BScrollView("editorScrollview", fTextView, 0, true, true);

    auto layout = BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0)
		.SetInsets(0.0)
        .Add(fScrollView)
        .Add(fStatusBar).Layout();

    BSize min = fScrollView->MinSize();
	BSize max = fScrollView->MaxSize();
	fScrollView->SetExplicitMinSize(min);
	fScrollView->SetExplicitMaxSize(max);
}

EditorView::~EditorView() {
    RemoveSelf();
    delete fColorDefs;
}

void EditorView::MessageReceived(BMessage* message) {
    switch (message->what) {
        case MSG_INSERT_ENTITY:
        {
            printf("EditorView::insert type:\n");
            const char* label = message->GetString(MSG_PROP_LABEL);
            if (label != NULL) {
                printf("will insert entity type %s.\n", label);
            }
            break;
        }
        case MSG_ADD_HIGHLIGHT:
        {
            printf("EditorView::add highlight for selection:\n");
            const char* label = message->GetString(MSG_PROP_LABEL);
            if (label != NULL) {
                printf("highlight with label %s\n", label);
                // calculate highlight color
                uint32 hash = BString(label).HashValue();
                int colorIndex = (hash >> 2) % NUM_COLORS - 1;

                printf("=== highlighting with screen color #%d.\n", colorIndex);
                const rgb_color *col = fColorDefs->GetColor(static_cast<COLOR_NAME>(colorIndex));
                fTextView->HighlightSelection(NULL, col);
            }
            break;
        }
        default:
        {
            BView::MessageReceived(message);
            break;
        }
    }
}

void EditorView::SetText(BFile* file, size_t size) {
    fTextView->SetText(file, 0, size);
}

void EditorView::SetText(const char* text) {
    fTextView->SetText(text);
}
