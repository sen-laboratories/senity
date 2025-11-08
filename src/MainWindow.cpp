/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MainWindow.h"
#include "common/Messages.h"

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

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Window"

static const char* kSettingsFile = "senity_settings";

const char* testMarkdownText = R"(# Welcome to SENity

## A semantic editor for your thoughts

This is a **bold** statement with some `inline code`.

Link to [se docs](http://sen.docs.org).

* [ ] some task
* [x] some completed task

## Code Example

Here's some C++ code:

```cpp
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
```

## Python Example

```python
def hello_world():
    print("Hello, World!")
    return True
```

### Features

- Syntax highlighting
- Outline navigation
- Fast incremental parsing
)";

MainWindow::MainWindow()
	:
	BWindow(BRect(100.0, 100.0, 260.0, 480.0), B_TRANSLATE("New Note"), B_DOCUMENT_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
{
	BMenuBar* menuBar = BuildMenu();
    fEditorView = new EditorView(this);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0.0)
		.SetInsets(0.0)
        .Add(menuBar)
        .Add(fEditorView);

    fSettings = new BMessage(MSG_SETTINGS);
	status_t status = LoadSettings(fSettings);

    if (status != B_OK) {
        printf("error loading settings, using defaults.");
    }

	BRect frame;
	if (fSettings->FindRect("main_window_rect", &frame) == B_OK) {
		MoveTo(frame.LeftTop());
		ResizeTo(frame.Width(), frame.Height());
        MoveOnScreen();
	} else {
        ResizeTo(320.0, 480.0);
        CenterOnScreen();
        frame = Frame();
    }

    // panels
    BMessenger messenger(this);
	fOpenPanel = new BFilePanel(B_OPEN_PANEL, &messenger, NULL, B_FILE_NODE, false);
	fSavePanel = new BFilePanel(B_SAVE_PANEL, &messenger, NULL, B_FILE_NODE, false);

    // Create outline panel (initially hidden)
    BMessenger editorMessenger(fEditorView);
    BRect panelFrame(frame.left - 240.0, frame.top, frame.left - 12.0, frame.bottom);

    fOutlinePanel = new OutlinePanel(panelFrame, &editorMessenger);
    fOutlinePanel->Show();

    ApplySettings(fSettings);
}

MainWindow::~MainWindow()
{
	SaveSettings(fSettings);

	delete fOpenPanel;
	delete fSavePanel;
    delete fEditorView;

    if (fOutlinePanel && LockLooper()) {
        delete fOutlinePanel;
        UnlockLooper();
    }
    Quit();
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

            break;
		}

		case MSG_FILE_NEW:
		{
			fSaveMenuItem->SetEnabled(false);
            fEditorView->SetText(testMarkdownText);

            break;
		}

		case MSG_FILE_OPEN:
		{
			fOpenPanel->Show();
            break;
		}

		case MSG_FILE_SAVE:
		{
			fSavePanel->Show();
            break;
		}

        // panels
        case MSG_OUTLINE_TOGGLE:
        {
            printf("toggle outline...\n");
            bool show = ! fSettings->GetBool(CONF_PANEL_OUTLINE_SHOW);
            fSettings->SetBool(CONF_PANEL_OUTLINE_SHOW, show);

            if (show)
                fOutlinePanel->Show();
            else
                fOutlinePanel->Hide();

            break;
        }
        case MSG_OUTLINE_UPDATE:
        {
            // Called after text changes
            if (fOutlinePanel && !fOutlinePanel->IsHidden()) {
                printf("MainWindow: UPDATE outline panel.\n");

                // get embedded outline from update event notification
                BMessage outline;
                status_t result = message->FindMessage("outline", &outline);

                if (result == B_OK) {
                    fOutlinePanel->UpdateOutline(&outline);
                } else {
                    printf("invalid outline update, no outline found!\n");
                }
            }
            break;
        }
        case MSG_OUTLINE_SELECTED: {
            // forward to textview
            printf("MainWindow: forward SELECTION from outline panel.\n");
            fEditorView->MessageReceived(message);
            break;
        }
        case MSG_SELECTION_CHANGED: {
            // currently only affects outline position
            int32 offset = message->FindInt32("offsetStart");
            fOutlinePanel->HighlightCurrent(offset);

            break;
        }
		default:
		{
			BWindow::MessageReceived(message);
			break;
		}
	}
}

BMenuBar* MainWindow::BuildMenu()
{
	BMenuBar* menuBar = new BMenuBar("menubar");
	BMenu* menu;
	BMenuItem* item;

	// menu 'File'
	menu = new BMenu(B_TRANSLATE("File"));

	item = new BMenuItem(B_TRANSLATE("New"), new BMessage(MSG_FILE_NEW), 'N');
	menu->AddItem(item);

	item = new BMenuItem(B_TRANSLATE("Open" B_UTF8_ELLIPSIS), new BMessage(MSG_FILE_OPEN), 'O');
	menu->AddItem(item);

	fSaveMenuItem = new BMenuItem(B_TRANSLATE("Save"), new BMessage(MSG_FILE_SAVE), 'S');
	fSaveMenuItem->SetEnabled(false);
	menu->AddItem(fSaveMenuItem);

	menu->AddSeparatorItem();

	item = new BMenuItem(B_TRANSLATE("About" B_UTF8_ELLIPSIS), new BMessage(B_ABOUT_REQUESTED));
	item->SetTarget(be_app);
	menu->AddItem(item);

	item = new BMenuItem(B_TRANSLATE("Quit"), new BMessage(B_QUIT_REQUESTED), 'Q');
	menu->AddItem(item);

	menuBar->AddItem(menu);

	// TODO: menu 'Edit'

	// TODO: menu 'View'

    // menu 'Panels'
	menu = new BMenu(B_TRANSLATE("Panels"));

	fOutlinePanelItem = new BMenuItem(B_TRANSLATE("Outline"),
                                      new BMessage(MSG_OUTLINE_TOGGLE), 'O', B_OPTION_KEY);
    menu->AddItem(fOutlinePanelItem);

	menuBar->AddItem(menu);

	return menuBar;
}

status_t MainWindow::LoadSettings(BMessage* settings)
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

	return settings->Unflatten(&file);
}

status_t MainWindow::SaveSettings(BMessage* settings)
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

	return settings->Flatten(&file);
}

void MainWindow::ApplySettings(BMessage* settings)
{
    printf("Apply settings...\n");
    settings->PrintToStream();

    bool show = settings->GetBool(CONF_PANEL_OUTLINE_SHOW);
	fOutlinePanelItem->SetMarked(show);
    if (!show)
        fOutlinePanel->Hide();
}
