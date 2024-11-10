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

    fStatusBar = new StatusBar();

	BRect viewFrame = Bounds();
	BRect textBounds = viewFrame;
	textBounds.OffsetTo(B_ORIGIN);

    fTextView = new EditorTextView(viewFrame, textBounds, fStatusBar, this);
    fTextView->SetStylable(true);
    fTextView->SetDoesUndo(true);
    fTextView->SetWordWrap(false);
    fTextView->SetFontAndColor(be_plain_font);
    fTextView->SetText("Welcome to SENity!");   // this forces a relayout and avoids a grey border to the right

    fScrollView = new BScrollView("scrollview", fTextView, B_FOLLOW_ALL, 0, true, true);

    BLayoutBuilder::Group<>((BGroupLayout*)GetLayout())
        .Add(fScrollView)
        .Add(fStatusBar)
        .End();
}

EditorView::~EditorView() {
    RemoveSelf();
    delete fStatusBar;
    delete fScrollView;
}

void EditorView::SetText(BFile* file, size_t size) {
    fTextView->SetText(file, 0, size);
}
