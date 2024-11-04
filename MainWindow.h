/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <FilePanel.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <TextView.h>
#include <Window.h>

#include "MarkdownStyler.h"

class MainWindow : public BWindow
{
public:
							MainWindow();
	virtual					~MainWindow();

	virtual void			MessageReceived(BMessage* msg);

private:
			BMenuBar*		_BuildMenu();

			status_t		_LoadSettings(BMessage& settings);
			status_t		_SaveSettings();

            void            MarkupText(int32 start = 0, int32 end = -1);

			BMenuItem*		fSaveMenuItem;
			BFilePanel*		fOpenPanel;
			BFilePanel*		fSavePanel;
            BTextView*      fEditorView;
            MarkdownStyler* fMarkdownStyler;
};
