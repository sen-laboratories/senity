/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <FilePanel.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <TextControl.h>
#include <Window.h>

#include "editor/EditorView.h"
#include "panels/outline/OutlinePanel.h"

class MainWindow : public BWindow
{
public:
							MainWindow(const BMessage* settings);
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* msg);
    BMessage*               GetWindowSettings() { return fSettings; };

private:
    void                    ApplySettings(BMessage* settings);
    BMenuBar*		        BuildMenu();

    BMessage*               fSettings;
    BMenuItem*		        fSaveMenuItem;
    // panels
    BMenuItem*              fOutlinePanelItem;

    BFilePanel*		        fOpenPanel;
    BFilePanel*		        fSavePanel;
    OutlinePanel*           fOutlinePanel;

    EditorView*             fEditorView;
};
