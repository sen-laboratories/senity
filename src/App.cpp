/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <Catalog.h>

#include "AboutWindow.h"
#include "App.h"
#include "MainWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Application"

const char* kApplicationSignature = "application/x-vnd.senlabs-senity";

App::App()
	:
	BApplication(kApplicationSignature)
{
    MainWindow* mainWindow = new MainWindow();
	mainWindow->Show();
}


App::~App()
{
}


void App::AboutRequested()
{
	BAboutWindow* about = new BAboutWindow(B_TRANSLATE_SYSTEM_NAME("SENity"), kApplicationSignature);
	about->AddDescription(B_TRANSLATE("A semantic notepad for your thoughts."));
	about->AddCopyright(2025, "Gregor B. Rosenauer");
	about->Show();
}

void App::ArgvReceived(int32 argc, char ** argv) {
    BMessage refsMsg(B_REFS_RECEIVED);

    // currently we only treat the 1st arg as file name if it exists, else we just open a new document
    if (argc == 0) {
        // TODO: quick hack, needs proper window management and sending  a NEW_DOCUMENT message
        MainWindow* mainWindow = new MainWindow();
        mainWindow->Show();
    } else {
        BEntry entry(argv[1]);
        entry_ref ref;

        entry.GetRef(&ref);
        refsMsg.AddRef("refs", &ref);

        RefsReceived(&refsMsg);
    }
}

int main(int32 argc, char **argv)
{
	App* app = new App();
	app->Run();

	delete app;
	return 0;
}
