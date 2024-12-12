/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <LayoutBuilder.h>
#include <SeparatorView.h>
#include <string>
#include "StatusBar.h"

StatusBar::StatusBar() : BView("status_bar", 0) {
    fLine = new BTextControl("line", "-", new BMessage('line'));
    fColumn = new BTextControl("column", "-", new BMessage('clmn'));
    fOffset = new BTextControl("offset", "-", new BMessage('offs'));
    fSelection = new BTextControl("selection", "-", new BMessage('slct'));
    fSelection->SetEnabled(false);
    fSelection->SetExplicitMinSize(BSize(64.0, be_plain_font->Size()));

    fOutline = new BStringView("outline", "-");
    fOutline->SetExplicitMinSize(BSize(180.0, be_plain_font->Size()));

	BLayoutBuilder::Group<>(this, B_HORIZONTAL)
		.Add(fLine)
        .Add(fColumn)
        .Add(fOffset)
        .Add(fSelection)
        .Add(new BStringView("outlineLabel", "Outline"))
        .Add(fOutline)
        .AddGlue(1.0);

    UpdatePosition(0, 1, 0);
    UpdateSelection(0, 0);
}

StatusBar::~StatusBar() {
    delete fLine;
    delete fColumn;
    delete fOffset;
    delete fSelection;
    delete fOutline;
}

void StatusBar::UpdatePosition(int32 offset, int32 line, int32 column) {
    fOffset ->SetText(std::to_string(offset).c_str());
    fLine   ->SetText(std::to_string(line  ).c_str());
    fColumn ->SetText(std::to_string(column).c_str());
}

void StatusBar::UpdateSelection(int32 selectionStart, int32 selectionEnd) {
    BString selection("");
    if (selectionStart != selectionEnd) {
        selection << selectionStart << " - " << selectionEnd
                  << " (" << (selectionEnd - selectionStart) << " chars)";
    } else {
        selection << "-";
    }
    fSelection->SetText(selection.String());
}

void StatusBar::UpdateOutline(const BMessage* outlineItems) {
    if (outlineItems == NULL || outlineItems->IsEmpty()) {
        fOutline->SetText("");
        printf("no outline to show.\n");
        return;
    }

    BString outline;
    int32 *count;

    for (int i = 0; i < outlineItems->CountNames(B_POINTER_TYPE); i++) {
        char  *key[B_ATTR_NAME_LENGTH];
        type_code type;
        status_t result = outlineItems->GetInfo(B_POINTER_TYPE, i, key, &type, NULL);
        if (result != B_OK) {
            printf("error processing text outline: %s\n", strerror(result));
            fOutline->SetText("???");
            return;
        }
        if (i > 0) {
            // add separator
            outline << " > ";
        }
        outline << *key;
    }
    printf("outline: %s\n", outline.String());
    fOutline->SetText(outline.String());
}
