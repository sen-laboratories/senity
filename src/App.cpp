/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include <Catalog.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <SupportDefs.h>

#include "AboutWindow.h"
#include "App.h"
#include "MainWindow.h"

#include "common/Messages.h"

#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Application"

static const char* kSettingsFile = "senity_settings";

const char* kApplicationSignature = "application/x-vnd.senlabs-senity";

int main(int32 argc, char **argv)
{
	App* app = new App();
	app->Run();

	delete app;
	return 0;
}

App::App()
	:
	BApplication(kApplicationSignature),
    fSettings(new BMessage(MSG_SETTINGS))
{
    auto console = spdlog::stdout_color_mt("senity");
    spdlog::set_default_logger(console);

	status_t status = LoadSettings(fSettings);
    if (status != B_OK) {
        spdlog::warn("error loading settings, using defaults.");
    }

    ApplySettings(fSettings);

    // todo: apply log level from settings
    // built in default
    spdlog::set_level(spdlog::level::debug);

    // settings can be overridden by environment
    spdlog::cfg::load_env_levels();

    // todo: handle window roster
    MainWindow* mainWindow = new MainWindow(fSettings);
    atomic_add(&fWindowCount, 1);
	mainWindow->Show();
}


App::~App()
{
	SaveSettings(fSettings);
}

status_t App::LoadSettings(BMessage* settings)
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

status_t App::SaveSettings(BMessage* settings)
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

    spdlog::debug("saving global settings to {}", path.Path());
	return settings->Flatten(&file);
}

void App::ApplySettings(BMessage* settings)
{
    spdlog::debug("Apply global settings...");

    if (spdlog::default_logger()->should_log(spdlog::level::debug)) {
        settings->PrintToStream();
    }
    // todo: use BObserver to inform windows
}

void App::AboutRequested()
{
    BAboutWindow* about = new BAboutWindow(B_TRANSLATE_SYSTEM_NAME("SENity"), kApplicationSignature);
	about->AddDescription(B_TRANSLATE("A semantic notepad for your thoughts."));
	about->AddCopyright(2025, "Gregor B. Rosenauer");
	about->Show();
}

void App::ArgvReceived(int32 argc, char ** argv)
{
    BMessage refsMsg(B_REFS_RECEIVED);

    // currently we only treat the 1st arg as file name if it exists, else we just open a new document
    if (argc == 0) {
        // TODO: quick hack, needs proper window management and sending  a NEW_DOCUMENT message
        MainWindow* mainWindow = new MainWindow(fSettings);
        mainWindow->Show();
    } else {
        BEntry entry(argv[1]);
        entry_ref ref;

        entry.GetRef(&ref);
        refsMsg.AddRef("refs", &ref);

        RefsReceived(&refsMsg);
    }
}

void App::MessageReceived(BMessage* message)
{
    switch(message->what) {
        case MSG_WINDOW_CLOSED: {
            atomic_add(&fWindowCount, -1);
            if (fWindowCount == 0) {
                Quit();
            }
            break;
        }
        default:
            BApplication::MessageReceived(message);
    }
}

