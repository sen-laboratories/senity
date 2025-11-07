/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "OutlinePanel.h"
#include "EditorTextView.h"

#include <LayoutBuilder.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <String.h>

// OutlineItem implementation

OutlineItem::OutlineItem(const char* text, int32 level, int32 offset)
    : BStringItem(text)
    , fLevel(level)
    , fOffset(offset)
{
}

void OutlineItem::DrawItem(BView* owner, BRect frame, bool complete)
{
    rgb_color bgColor;
    
    if (IsSelected()) {
        bgColor = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
    } else {
        bgColor = ui_color(B_LIST_BACKGROUND_COLOR);
    }
    
    owner->SetHighColor(bgColor);
    owner->SetLowColor(bgColor);
    owner->FillRect(frame);
    
    // Indent based on heading level
    float indent = (fLevel - 1) * 12.0f;
    
    // Draw text
    owner->SetHighColor(IsSelected() 
        ? ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR)
        : ui_color(B_LIST_ITEM_TEXT_COLOR));
    
    BFont font;
    owner->GetFont(&font);
    
    // Make H1/H2 bold
    if (fLevel <= 2) {
        font.SetFace(B_BOLD_FACE);
        owner->SetFont(&font);
    }
    
    font_height fh;
    font.GetHeight(&fh);
    float textY = frame.top + fh.ascent + 
                  (frame.Height() - (fh.ascent + fh.descent)) / 2.0f;
    
    owner->DrawString(Text(), BPoint(frame.left + indent + 4, textY));
}

// OutlineListView implementation

OutlineListView::OutlineListView()
    : BListView("outline_list", B_SINGLE_SELECTION_LIST)
    , fTarget(nullptr)
{
}

void OutlineListView::SelectionChanged()
{
    BListView::SelectionChanged();
    
    if (!fTarget)
        return;
    
    int32 index = CurrentSelection();
    if (index < 0)
        return;
    
    OutlineItem* item = dynamic_cast<OutlineItem*>(ItemAt(index));
    if (!item)
        return;
    
    BMessage msg(MSG_OUTLINE_SELECTED);
    msg.AddInt32("offset", item->GetOffset());
    
    BMessenger messenger(fTarget);
    messenger.SendMessage(&msg);
}

// OutlinePanel implementation

OutlinePanel::OutlinePanel(BRect frame, EditorTextView* textView)
    : BWindow(frame, "Outline", B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
              B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
    , fTextView(textView)
{
    // Create list view
    fListView = new OutlineListView();
    fListView->SetTarget(textView);
    
    // Wrap in scroll view
    fScrollView = new BScrollView("outline_scroll", fListView,
                                  0, false, true, B_NO_BORDER);
    
    // Layout
    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(fScrollView)
    .End();
    
    // Initial update
    UpdateOutline();
}

OutlinePanel::~OutlinePanel()
{
}

void OutlinePanel::MessageReceived(BMessage* message)
{
    switch (message->what) {
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool OutlinePanel::QuitRequested()
{
    Hide();
    return false;  // Don't actually quit, just hide
}

void OutlinePanel::UpdateOutline()
{
    if (!fTextView || !fListView)
        return;
    
    // Clear existing items
    fListView->MakeEmpty();
    
    // Get document outline from parser
    BMessage* outline = fTextView->GetDocumentOutline(true, false);
    if (!outline || outline->what != 'OUTL')
        return;
    
    // Add all headings to list
    for (int i = 0; ; i++) {
        BMessage heading;
        if (outline->FindMessage("heading", i, &heading) != B_OK)
            break;
        
        BString text;
        int32 level = 1;
        int32 offset = 0;
        
        heading.FindString("text", &text);
        heading.FindInt32("level", &level);
        heading.FindInt32("offset", &offset);
        
        // Add item with indentation
        OutlineItem* item = new OutlineItem(text.String(), level, offset);
        fListView->AddItem(item);
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
        
        int32 itemOffset = item->GetOffset();
        
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
