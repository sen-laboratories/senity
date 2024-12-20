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
}

void EditorView::SetText(BFile* file, size_t size) {
    fTextView->SetText(file, 0, size);
}
