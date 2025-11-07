/*
 * Copyright 2024-2025, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef OUTLINE_PANEL_H
#define OUTLINE_PANEL_H

#include <Window.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>

class EditorTextView;

class OutlineItem : public BStringItem {
public:
    OutlineItem(const char* text, int32 level, int32 offset);
    
    virtual void DrawItem(BView* owner, BRect frame, bool complete = false);
    
    int32 GetOffset() const { return fOffset; }
    int32 GetLevel() const { return fLevel; }

private:
    int32 fLevel;
    int32 fOffset;
};

class OutlineListView : public BListView {
public:
    OutlineListView();
    
    virtual void SelectionChanged();
    
    void SetTarget(BHandler* target) { fTarget = target; }

private:
    BHandler* fTarget;
};

class OutlinePanel : public BWindow {
public:
    OutlinePanel(BRect frame, EditorTextView* textView);
    virtual ~OutlinePanel();
    
    virtual void MessageReceived(BMessage* message);
    virtual bool QuitRequested();
    
    void UpdateOutline();
    void HighlightCurrent(int32 offset);

private:
    EditorTextView* fTextView;
    OutlineListView* fListView;
    BScrollView* fScrollView;
};

#define MSG_OUTLINE_SELECTED 'outS'

#endif // OUTLINE_PANEL_H
