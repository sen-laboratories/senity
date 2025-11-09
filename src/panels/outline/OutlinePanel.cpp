/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "OutlinePanel.h"
#include "../../common/Messages.h"
#include "../../editor/EditorTextView.h"

#include <Application.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <String.h>
#include <stdio.h>

// OutlineListView implementation

void OutlineListView::ExpandAll() {
    for (int32 i = 0; i < FullListCountItems(); i++) {
        BListItem* item = FullListItemAt(i);
        if (ItemUnderAt(item, true, 0) != NULL) {   // it's a parent node
            Expand(item);
        }
    }
}

void OutlineListView::CollapseAll() {
    for (int32 i = 0; i < FullListCountItems(); i++) {
        BListItem* item = FullListItemAt(i);
        if (ItemUnderAt(item, true, 0) != NULL) {   // it's a parent node
            Collapse(item);
        }
    }
}

void OutlineListView::SelectionChanged()
{
    if (fSuppressSelectionChanged) {
        printf("ignoring selection change.\n");
        return;
    }

    int32 index = FullListCurrentSelection();
    if (index < 0)
        return;

    OutlineItem* item = dynamic_cast<OutlineItem*>(FullListItemAt(index));
    if (!item) {
        printf("could not get itemm at index %d\n", index);
        return;
    }

    printf("Outline: sending selection update msg.\n");

    BMessage selectionMsg(MSG_OUTLINE_SELECTED);
    selectionMsg.AddInt32("offsetStart", item->Offset());
    selectionMsg.AddInt32("offsetEnd", item->Offset());

    SetSelectionMessage(new BMessage(selectionMsg));
    BOutlineListView::SelectionChanged();
}

// OutlinePanel implementation

OutlinePanel::OutlinePanel(BRect frame, BMessenger* target)
    : BWindow(frame, "Outline", B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
              B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS |
              B_NOT_ZOOMABLE | B_AVOID_FRONT | B_WILL_ACCEPT_FIRST_CLICK)
{
    // Create custom list view
    fListView = new OutlineListView();
    fListView->SetTarget(*target);

    // Wrap in scroll view
    fScrollView = new BScrollView("outline_scroll", fListView,
                                  0, false, true, B_NO_BORDER);

    // Layout
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(fScrollView)
    .End();
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
                Show();
            } else {
                printf("OutlinePanel: HIDE outline panel.\n");
                Hide();
            }
            break;
        }
        case MSG_OUTLINE_UPDATE:
        {
            BMessage outline;
            if (message->FindMessage("outline", &outline) == B_OK) {
                printf("OutlinePanel: UPDATE outline.\n");
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
    be_app_messenger.SendMessage(MSG_OUTLINE_TOGGLE);  // use MSG_PANEL_TOGGLE with unique ID
    return true;
}

void OutlinePanel::UpdateOutline(BMessage* outline)
{
    if (!outline) return;

    printf("OutlinePanel::UpdateOutline\n");

    if (LockLooper()) {
        fListView->SuppressSelectionChanged(true);
        fListView->MakeEmpty();
        UnlockLooper();
    }

    // Check if we have headings
    int32 count = 0;
    if (outline->IsEmpty() || outline->GetInfo("heading", NULL, &count) != B_OK || count == 0) {
        printf("document is empty or has no headings.\n");
        return;
    }

    printf("Adding %d headings\n", count);

    // Use simple flat iteration with parent tracking
    AddHeadingsFlat(outline);
    fListView->SuppressSelectionChanged(false);
}

void OutlinePanel::AddHeadingsFlat(BMessage* outline)
{
    int32 count = 0;
    outline->GetInfo("heading", NULL, &count);

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

        // Add to tree
        if (LockLooper()) {
            fListView->AddItem(item);
            UnlockLooper();
        }
    }
}

void OutlinePanel::HighlightCurrent(int32 offset)
{
    if (!fListView)
        return;

    printf("highlight current outline item.\n");

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
            // don't send selection change event
            fListView->SuppressSelectionChanged(true);
            fListView->Select(bestIndex);

            // expand item if collapsed
            BListItem* item = fListView->FullListItemAt(bestIndex);

            if (! item->IsExpanded()) {
                if (LockLooper()) {
                    fListView->Expand(item);
                    UnlockLooper();
                }
            }
            fListView->ScrollToSelection();

            // Re-enable selection messages
            fListView->SuppressSelectionChanged(false);
            UnlockLooper();
        }
    }

}
