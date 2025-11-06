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
        return;
    }

    // Get count of heading messages
    type_code type;
    int32 count = 0;
    if (outlineItems->GetInfo("heading", &type, &count) != B_OK || count == 0) {
        fOutline->SetText("-");
        return;
    }

    BString outline;

    // Iterate through heading messages
    for (int32 i = 0; i < count; i++) {
        BMessage heading;
        if (outlineItems->FindMessage("heading", i, &heading) != B_OK) {
            continue;
        }

        const char* text = NULL;
        int32 level = 0;

        heading.FindString("text", &text);
        heading.FindInt32("level", &level);

        if (text == NULL || strlen(text) == 0) {
            continue;
        }

        // Shorten name for outline display
        BString shortName(text);
        if (shortName.Length() > 20) {
            shortName.Truncate(19);
            shortName.Append(B_UTF8_ELLIPSIS);
        }

        // Add separator before each item except first
        if (i > 0) {
            outline << " > ";
        }

        outline << shortName;
    }

    if (outline.IsEmpty()) {
        outline = "-";
    }

    fOutline->SetText(outline.String());
}
