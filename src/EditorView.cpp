/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <ObjectList.h>
#include <stdio.h>

#include "EditorView.h"

EditorView::EditorView() : BView("editor_view", B_WILL_DRAW | B_PULSE_NEEDED | B_FRAME_EVENTS) {

    fStatusBar = new StatusBar();
    fTextView = new EditorTextView(fStatusBar, this);
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
    fScrollView->SetExplicitMaxSize(BSize(fScrollView->Bounds().Width(), fScrollView->Bounds().Height()));
    fTextView->SetText(file, 0, size);
}
