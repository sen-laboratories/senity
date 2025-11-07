/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "OutlinePanel.h"
#include "../../common/Messages.h"
#include "../../editor/EditorTextView.h"

#include <LayoutBuilder.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <String.h>
#include <stdio.h>

// OutlineListView implementation

void OutlineListView::SelectionChanged()
{
    BOutlineListView::SelectionChanged();

    if (fSuppressSelectionMessage)
        return;

    int32 index = CurrentSelection();
    if (index < 0)
        return;

    OutlineItem* item = dynamic_cast<OutlineItem*>(ItemAt(index));
    if (!item)
        return;

    BMessage selectionMsg(MSG_OUTLINE_SELECTED);
    selectionMsg.AddInt32("offsetStart", item->Offset());
    selectionMsg.AddInt32("offsetEnd", item->Offset());

    if (fTargetMessenger) {
        fTargetMessenger->SendMessage(&selectionMsg);
    }
}

// OutlinePanel implementation

OutlinePanel::OutlinePanel(BRect frame, BMessenger* target)
    : BWindow(frame, "Outline", B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
              B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS |
              B_NOT_ZOOMABLE | B_AVOID_FRONT | B_WILL_ACCEPT_FIRST_CLICK)
    , fTargetMessenger(target)
{
    printf("OutlinePanel initializing...\n");

    // Create custom list view
    fListView = new OutlineListView(target);

    // Wrap in scroll view
    fScrollView = new BScrollView("outline_scroll", fListView,
                                  0, false, true, B_NO_BORDER);

    // Layout
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(fScrollView)
    .End();

    printf("OutlinePanel initialized.\n");
}

OutlinePanel::~OutlinePanel()
{
}

void OutlinePanel::MessageReceived(BMessage* message)
{
    printf("OutlinePanel::MessageReceived\n");

    switch (message->what) {
        case MSG_OUTLINE_TOGGLE:
        {
            bool show = message->GetBool("show");
            if (show) {
                printf("OutlinePanel: SHOW outline panel.\n");
                //Show();
            } else {
                printf("OutlinePanel: HIDE outline panel.\n");
                //Hide();
            }
            break;
        }
        case MSG_OUTLINE_UPDATE:
        {
            BMessage outline;
            if (message->FindMessage("outline", &outline) == B_OK) {
                UpdateOutline(&outline);
            }
            break;
        }
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool OutlinePanel::QuitRequested()
{
    fTargetMessenger->SendMessage(MSG_OUTLINE_TOGGLE);
    return false;  // Don't actually quit, just hide
}

void OutlinePanel::UpdateOutline(BMessage* outline)
{
    if (!outline) return;

    printf("OutlinePanel::UpdateOutline\n");

    fListView->MakeEmpty();

    // Check if we have headings
    type_code type;
    int32 count = 0;
    if (outline->GetInfo("heading", &type, &count) != B_OK || count == 0) {
        printf("No headings in outline\n");
        return;
    }

    printf("Adding %d headings\n", count);

    // Use simple flat iteration with parent tracking
    AddHeadingsFlat(outline);
}

void OutlinePanel::AddHeadingsFlat(BMessage* outline)
{
    type_code type;
    int32 count = 0;
    outline->GetInfo("heading", &type, &count);

    OutlineItem* parents[7] = {nullptr};  // H1-H6, index by level
    int32 lastLevel = 0;

    for (int32 i = 0; i < count; i++) {
        BMessage heading;
        if (outline->FindMessage("heading", i, &heading) != B_OK) {
            printf("Failed to get heading %d\n", i);
            continue;
        }

        BString text;
        int32 level, offset;
        heading.FindString("text", &text);
        heading.FindInt32("level", &level);
        heading.FindInt32("offset", &offset);

        printf("Adding H%d: %s at offset %d\n", level, text.String(), offset);

        // Create item with outline level (0-based for BOutlineListView)
        OutlineItem* item = new OutlineItem(text, offset, level - 1);

        // Clear parent pointers for levels >= current level
        for (int32 j = level; j <= 6; j++) {
            parents[j] = nullptr;
        }

        // Find parent (first non-null parent at level < current)
        OutlineItem* parent = nullptr;
        for (int32 j = level - 1; j >= 1; j--) {
            if (parents[j]) {
                parent = parents[j];
                break;
            }
        }

        // Add to tree
        if (LockLooper()) {
            if (parent) {
                fListView->AddUnder(item, parent);
            } else {
                fListView->AddItem(item);
            }
            UnlockLooper();
        }
        // This item becomes the parent for its level
        parents[level] = item;
        lastLevel = level;
    }

    printf("OutlinePanel: Added all headings\n");
}

void OutlinePanel::HighlightCurrent(int32 offset)
{
    if (!fListView)
        return;

    fListView->SetSuppressSelectionMessage(true);

    // Find item closest to offset (last heading before or at offset)
    int32 bestIndex = -1;
    int32 bestOffset = -1;

    int32 count = fListView->FullListCountItems();
    for (int32 i = 0; i < count; i++) {
        OutlineItem* item = dynamic_cast<OutlineItem*>(fListView->FullListItemAt(i));
        if (!item)
            continue;

        int32 itemOffset = item->Offset();

        if (itemOffset <= offset) {
            if (bestOffset < 0 || itemOffset > bestOffset) {
                bestOffset = itemOffset;
                bestIndex = i;
            }
        }
    }

    // Select the item
    if (bestIndex >= 0) {
        if (LockLooper()) {
            fListView->Select(bestIndex);
            fListView->ScrollToSelection();
            UnlockLooper();
        }
    }

    // Re-enable selection messages
    fListView->SetSuppressSelectionMessage(false);
}
