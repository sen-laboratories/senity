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
							MainWindow();
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* msg);

private:
			BMenuBar*		BuildMenu();

			status_t		LoadSettings(BMessage& settings);
			status_t		SaveSettings();

			BMenuItem*		fSaveMenuItem;
            // panels
            BMenuItem*      fOutlinePanelItem;

			BFilePanel*		fOpenPanel;
			BFilePanel*		fSavePanel;
            OutlinePanel*   fOutlinePanel;

            EditorView*     fEditorView;
};
