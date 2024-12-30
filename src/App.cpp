/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <AboutWindow.h>
#include <Catalog.h>
#include <iostream>
#include <glog/logging.h>
#include <gflags/gflags.h>

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
	about->AddDescription(B_TRANSLATE("A simple semantic notepad for your thoughts."));
	about->AddCopyright(2024, "Gregor B. Rosenauer");
	about->Show();
}

void App::ArgvReceived(int32 argc, char ** argv) {
    // parse args, omitting logger args
    /*or (int32 optionIdx = 0; optionIdx < argc; optionIdx++) {
        switch (argv[optionIdx]) {
        }
    }*/

    if (argc < 1) {
        std::cerr << "Invalid usage, please provide at leas a file as argument." << std::endl;
        return;
    }

    BMessage refsMsg(B_REFS_RECEIVED);
    BEntry entry(argv[1]);
    entry_ref ref;

    entry.GetRef(&ref);
    refsMsg.AddRef("refs", &ref);

    RefsReceived(&refsMsg);
}

int main(int32 argc, char ** argv)
{
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging("SENity");
    LOG(INFO) << "SENity starting up." << std::endl;

	App* app = new App();
	app->Run();

	delete app;
	return 0;
}
