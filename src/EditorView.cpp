/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <ObjectList.h>
#include <stdio.h>

#include "EditorView.h"

EditorView::EditorView() : BView("editor_view", B_WILL_DRAW | B_PULSE_NEEDED | B_FRAME_EVENTS) {

    fStatusBar  = new StatusBar();
    fTextView   = new EditorTextView(fStatusBar, this);
    fScrollView = new BScrollView("editorScrollview", fTextView, B_WILL_DRAW, true, true);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0)
		.SetInsets(0.0)
        .Add(fScrollView)
        .Add(fStatusBar)
    .End();
}

EditorView::~EditorView() {
    RemoveSelf();
}

void EditorView::SetText(BFile* file, size_t size) {
    BRect frame(fScrollView->Bounds());
    fTextView->SetText(file, 0, size);
    // we need to adjust scrollbar ranges to keep the view layout in line
    BRect textRect = fTextView->TextRect();

	if (BScrollBar *scrollBar = fScrollView->ScrollBar(B_VERTICAL)) {
        float proportion = frame.Height() / textRect.Height();
		scrollBar->SetProportion(proportion);
		scrollBar->SetRange(0, textRect.Height());
        scrollBar->SetSteps(textRect.Height() / 8.0, textRect.Height() / 2.0);
	}
	if (BScrollBar *scrollBar = fScrollView->ScrollBar(B_HORIZONTAL)) {
		scrollBar->SetProportion(frame.Width() / textRect.Width());
		scrollBar->SetRange(0, textRect.Width());
        scrollBar->SetSteps(textRect.Width() / 8.0, textRect.Height() / 2.0);
	}
    Relayout();
}
