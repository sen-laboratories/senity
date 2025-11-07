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

/*
void OutlineListView::SelectionChanged()
{
    BOutlineListView::SelectionChanged();

    if (!fTarget)
        return;

    int32 index = CurrentSelection();
    if (index < 0)
        return;

    OutlineItem* item = dynamic_cast<OutlineItem*>(ItemAt(index));
    if (!item)
        return;

    BMessage msg(MSG_OUTLINE_SELECTED);
    msg.AddInt32("offset", item->Offset());

    BMessenger messenger(fTarget);
    messenger.SendMessage(&msg);
}
*/

OutlinePanel::OutlinePanel(BRect frame, BHandler* target)
    : BWindow(frame, "outline_window", B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
              B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
{
    printf("OutlinePanel initializing...\n");

    // Create list view
    fListView = new BOutlineListView("outline_listview");
    fListView->SetTarget(target);

    BMessage selectionMsg(MSG_OUTLINE_SELECTED);
    fListView->SetSelectionMessage(&selectionMsg);

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
    switch (message->what) {
        case MSG_OUTLINE_UPDATE:
            UpdateOutline(message);
            break;
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool OutlinePanel::QuitRequested()
{
    BMessenger(fTarget).SendMessage(MSG_OUTLINE_TOGGLE);
}

void OutlinePanel::UpdateOutline(BMessage* outline)
{
    if (!outline) return;

    printf("OutlinePanel::UpdateOutline:\n");

    fListView->MakeEmpty();

    // Check if we have headings
    type_code type;
    int32 count = 0;
    if (outline->GetInfo("heading", &type, &count) != B_OK || count == 0) {
        return;
    }

    // Recursively add headings
    OutlineItem* parents[7] = {nullptr};  // H1-H6
    int32 index = 0;
    AddHeadingsRecursive(outline, index, parents, 0);
}

void OutlinePanel::AddHeadingsRecursive(BMessage* outline, int32& index,
                                        OutlineItem** parents, int32 parentLevel)
{
    type_code type;
    int32 count = 0;
    outline->GetInfo("heading", &type, &count);

    while (index < count) {
        BMessage heading;
        if (outline->FindMessage("heading", index, &heading) != B_OK) {
            index++;
            continue;
        }

        BString text;
        int32 level, offset;
        heading.FindString("text", &text);
        heading.FindInt32("level", &level);
        heading.FindInt32("offset", &offset);

        // If level is same or lower than parent, we're done with this subtree
        if (level <= parentLevel) {
            return;
        }

        // Create item
        OutlineItem* item = new OutlineItem(text, offset, level);

        // Add to tree structure
        if (level == parentLevel + 1) {
            // Direct child
            if (parentLevel == 0) {
                fListView->AddItem(item);
            } else {
                fListView->AddUnder(item, parents[parentLevel]);
            }
            parents[level] = item;
            index++;

            // Process children recursively
            AddHeadingsRecursive(outline, index, parents, level);

        } else if (level > parentLevel + 1) {
            // Skipped levels - shouldn't happen with proper markdown, but handle it
            // by treating it as a child of current parent
            if (parentLevel == 0) {
                fListView->AddItem(item);
            } else {
                fListView->AddUnder(item, parents[parentLevel]);
            }
            parents[level] = item;
            index++;
        }
    }
}

void OutlinePanel::HighlightCurrent(int32 offset)
{
    if (!fListView)
        return;

    // Find item closest to offset
    int32 bestIndex = -1;
    int32 bestOffset = -1;

    for (int32 i = 0; i < fListView->CountItems(); i++) {
        OutlineItem* item = dynamic_cast<OutlineItem*>(fListView->ItemAt(i));
        if (!item)
            continue;

        int32 itemOffset = item->Offset();

        // Find the last heading before current offset
        if (itemOffset <= offset) {
            if (bestOffset < 0 || itemOffset > bestOffset) {
                bestOffset = itemOffset;
                bestIndex = i;
            }
        }
    }

    // Select the item
    if (bestIndex >= 0) {
        fListView->Select(bestIndex);
        fListView->ScrollToSelection();
    }
}
