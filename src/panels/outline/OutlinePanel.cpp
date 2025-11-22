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
#include <spdlog/spdlog.h>

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
        spdlog::debug("ignoring selection change.");
        return;
    }

    int32 index = FullListCurrentSelection();
    if (index < 0)
        return;

    OutlineItem* item = dynamic_cast<OutlineItem*>(FullListItemAt(index));
    if (!item) {
        spdlog::warn("could not get item at index {}", index);
        return;
    }

    spdlog::debug("sending selection update msg.");

    BMessage selectionMsg(MSG_OUTLINE_SELECTED);
    selectionMsg.AddInt32("offsetStart", item->Offset());
    selectionMsg.AddInt32("offsetEnd", item->Offset());
    selectionMsg.AddInt32("level", item->OutlineLevel());

    SetSelectionMessage(new BMessage(selectionMsg));
    BOutlineListView::SelectionChanged();
}

// OutlinePanel implementation

OutlinePanel::OutlinePanel(BRect frame, BMessenger* target)
    : BWindow(frame, "Outline", B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
              B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS |
              B_NOT_ZOOMABLE | B_AVOID_FRONT | B_WILL_ACCEPT_FIRST_CLICK)
{
    fTarget = target;

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
    spdlog::debug("MessageReceived");

    switch (message->what) {
        case MSG_OUTLINE_TOGGLE:
        {
            bool show = message->GetBool("show");
            if (show) {
                spdlog::debug("SHOW outline panel.");
                Show();
            } else {
                spdlog::debug("HIDE outline panel.");
                Hide();
            }
            break;
        }
        case MSG_OUTLINE_UPDATE:
        {
            BMessage outline;
            if (message->FindMessage("outline", &outline) == B_OK) {
                spdlog::debug("UPDATE outline.");
                UpdateOutline(&outline);
            }
            break;
        }
        case MSG_SELECTION_CHANGED: {
            int32 offset = message->FindInt32("offsetStart");
            // just update selection, don't back-propagate to editor!
            fListView->SuppressSelectionChanged(true);
                HighlightCurrent(offset);
            fListView->SuppressSelectionChanged(false);
        }
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool OutlinePanel::QuitRequested()
{
    fTarget->SendMessage(MSG_OUTLINE_TOGGLE);  // TODO: use MSG_PANEL_TOGGLE with unique ID
    Hide();
    return false;   // don't quit, just hide
}

void OutlinePanel::UpdateOutline(BMessage* outline)
{
    if (!outline) return;

    spdlog::debug("UpdateOutline");

    if (LockLooper()) {
        fListView->SuppressSelectionChanged(true);
        fListView->MakeEmpty();
        UnlockLooper();
    }

    // Check if we have headings
    int32 count = 0;
    if (outline->IsEmpty() || outline->GetInfo("heading", NULL, &count) != B_OK || count == 0) {
        spdlog::debug("document is empty or has no headings.");
        return;
    }

    spdlog::debug("Adding {} headings", count);

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
            spdlog::warn("Failed to get heading {}", i);
            continue;
        }

        BString text;
        int32 level, offset;
        heading.FindString("text", &text);
        heading.FindInt32("level", &level);
        heading.FindInt32("offset", &offset);

        spdlog::debug("Adding H{}: {} at offset {}", level, text.String(), offset);

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

    spdlog::debug("highlight current outline item.");

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
