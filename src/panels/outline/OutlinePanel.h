/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <Window.h>
#include <OutlineListView.h>
#include <ScrollView.h>
#include <StringItem.h>

class EditorTextView;

class OutlineItem : public BStringItem {
public:
    OutlineItem(const char* text, int32 offset, uint32 level = 0)
        : BStringItem(text, level, false)
        , fOffset(offset)
    {}

    int32 Offset() const { return fOffset; }

private:
    int32 fOffset;
};

class OutlinePanel : public BWindow {
public:
    OutlinePanel(BRect frame, BHandler* target);
    virtual ~OutlinePanel();

    virtual void MessageReceived(BMessage* message);
    virtual bool QuitRequested();

    void UpdateOutline(BMessage* outline);
    void HighlightCurrent(int32 offset);

private:
    void AddHeadingsRecursive(BMessage* outline, int32& index,
                              OutlineItem** parents, int32 parentLevel);

    BHandler*           fTarget;
    BScrollView*        fScrollView;
    BOutlineListView*   fListView;
};
