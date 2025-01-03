/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MainWindow.h"

#include <Application.h>
#include <Catalog.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <Path.h>
#include <View.h>

#include <cstdio>
#include <glog/logging.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Window"

static const uint32 kMsgNewFile = 'fnew';
static const uint32 kMsgOpenFile = 'fopn';
static const uint32 kMsgSaveFile = 'fsav';

static const char* kSettingsFile = "senity_settings";

MainWindow::MainWindow()
	:
	BWindow(BRect(100.0, 100.0, 260.0, 480.0), B_TRANSLATE("New Note"), B_DOCUMENT_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
{
	BMenuBar* menuBar = _BuildMenu();
    fEditorView = new EditorView();

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0)
		.SetInsets(0.0)
        .Add(menuBar)
        .Add(fEditorView);

	BMessenger messenger(this);
	fOpenPanel = new BFilePanel(B_OPEN_PANEL, &messenger, NULL, B_FILE_NODE, false);
	fSavePanel = new BFilePanel(B_SAVE_PANEL, &messenger, NULL, B_FILE_NODE, false);

	BMessage settings;
	_LoadSettings(settings);

	BRect frame;
	if (settings.FindRect("main_window_rect", &frame) == B_OK) {
		MoveTo(frame.LeftTop());
		ResizeTo(frame.Width(), frame.Height());
        MoveOnScreen();
	} else {
        ResizeTo(320.0, 480.0);
        CenterOnScreen();
    }
}

MainWindow::~MainWindow()
{
	_SaveSettings();

	delete fOpenPanel;
	delete fSavePanel;
    delete fEditorView;
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_SIMPLE_DATA:
		case B_REFS_RECEIVED:
		{
            printf("handing simple data/refs received msg.\n");

			entry_ref ref;
            status_t  result;

			if (message->FindRef("refs", &ref) != B_OK)
				break;

			fSaveMenuItem->SetEnabled(true); // todo only when changed

            // read all text from file
            BFile file(&ref, B_READ_WRITE);
            if (file.InitCheck() != B_OK) {
                // TODO: show alert with error
                break;
            }
            off_t size;
            if ((result = file.GetSize(&size)) != B_OK) {
                fprintf(stderr, "could not get size for file: %s\n", strerror(result));
                break;
            }

            // TODO: check MIME type
            // LATER: only load portion of file if above certain size
			fEditorView->SetText(&file, size);

            break;
		}

		case B_SAVE_REQUESTED:
		{
			entry_ref ref;
			const char* name;
			if (message->FindRef("directory", &ref) == B_OK
				&& message->FindString("name", &name) == B_OK) {
				BDirectory directory(&ref);
				BEntry entry(&directory, name);
				BPath path = BPath(&entry);

				printf("would save to path: %s\n", path.Path());
			}
		} break;

		case kMsgNewFile:
		{
			fSaveMenuItem->SetEnabled(false);
			printf("New\n");
		} break;

		case kMsgOpenFile:
		{
			fOpenPanel->Show();
		} break;

		case kMsgSaveFile:
		{
			fSavePanel->Show();
		} break;

		default:
		{
			BWindow::MessageReceived(message);
			break;
		}
	}
}


BMenuBar*
MainWindow::_BuildMenu()
{
	BMenuBar* menuBar = new BMenuBar("menubar");
	BMenu* menu;
	BMenuItem* item;

	// menu 'File'
	menu = new BMenu(B_TRANSLATE("File"));

	item = new BMenuItem(B_TRANSLATE("New"), new BMessage(kMsgNewFile), 'N');
	menu->AddItem(item);

	item = new BMenuItem(B_TRANSLATE("Open" B_UTF8_ELLIPSIS), new BMessage(kMsgOpenFile), 'O');
	menu->AddItem(item);

	fSaveMenuItem = new BMenuItem(B_TRANSLATE("Save"), new BMessage(kMsgSaveFile), 'S');
	fSaveMenuItem->SetEnabled(false);
	menu->AddItem(fSaveMenuItem);

	menu->AddSeparatorItem();

	item = new BMenuItem(B_TRANSLATE("About" B_UTF8_ELLIPSIS), new BMessage(B_ABOUT_REQUESTED));
	item->SetTarget(be_app);
	menu->AddItem(item);

	item = new BMenuItem(B_TRANSLATE("Quit"), new BMessage(B_QUIT_REQUESTED), 'Q');
	menu->AddItem(item);

	menuBar->AddItem(menu);

	return menuBar;
}


status_t
MainWindow::_LoadSettings(BMessage& settings)
{
	BPath path;
	status_t status;
	status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status != B_OK)
		return status;

	status = path.Append(kSettingsFile);
	if (status != B_OK)
		return status;

	BFile file;
	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status != B_OK)
		return status;

	return settings.Unflatten(&file);
}


status_t
MainWindow::_SaveSettings()
{
	BPath path;
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status != B_OK)
		return status;

	status = path.Append(kSettingsFile);
	if (status != B_OK)
		return status;

	BFile file;
	status = file.SetTo(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (status != B_OK)
		return status;

	BMessage settings;
	status = settings.AddRect("main_window_rect", Frame());

	if (status == B_OK)
		status = settings.Flatten(&file);

	return status;
}
