/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include <Alert.h>
#include <Application.h>
#include <Catalog.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Path.h>
#include <Screen.h>
#include <String.h>

#include <stdio.h>
#include <string.h>

#include "ColorWindow.h"
#include "Constants.h"
#include "ImageIO.h"
#include "PaintWindow.h"
#include "Settings.h"
#include "SettingsWindow.h"
#include "SizeWindow.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "App"


// Shows folders and images in the Open panel.
class ImageFilter : public BRefFilter {
public:
	virtual bool Filter(const entry_ref* ref, BNode* node,
		struct stat_beos* stat, const char* mimeType)
	{
		if (node->IsDirectory())
			return true;
		if (node->IsSymLink()) {
			BEntry entry(ref, true);
			return entry.IsDirectory() || entry.IsFile();
		}
		return mimeType != NULL && strncmp(mimeType, "image/", 6) == 0;
	}
};


class App : public BApplication {
public:
								App();
	virtual						~App();

	virtual	void				ArgvReceived(int32 argc, char** argv);
	virtual	void				RefsReceived(BMessage* message);
	virtual	void				ReadyToRun();
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				AboutRequested();
	virtual	bool				QuitRequested();

private:
			void				_OpenRef(const entry_ref& ref);
			void				_NewImage(int32 width, int32 height);
			void				_NewWindow(BBitmap* image,
									const entry_ref* ref);
			BRect				_NextFrame();
			void				_Broadcast(uint32 what);
			void				_Activate(BMessenger& messenger);

			BFilePanel*			fOpenPanel;
			ImageFilter			fImageFilter;
			int32				fWindowCount;
			int32				fCascade;
			BMessenger			fColorWindows[2];
			BMessenger			fSettingsWindow;
};


App::App()
	:
	BApplication(APP_SIGNATURE),
	fOpenPanel(NULL),
	fWindowCount(0),
	fCascade(0)
{
	// Fonts are only usable once the application exists.
	gSettings = new Settings();
	gSettings->Load();
}


App::~App()
{
	delete fOpenPanel;
}


void
App::ArgvReceived(int32 argc, char** argv)
{
	BMessage* message = CurrentMessage();
	const char* cwd = message != NULL ? message->GetString("cwd", NULL) : NULL;
	for (int32 i = 1; i < argc; i++) {
		BPath path;
		if (argv[i][0] != '/' && cwd != NULL)
			path.SetTo(cwd, argv[i]);
		else
			path.SetTo(argv[i]);
		BEntry entry(path.Path(), true);
		entry_ref ref;
		if (!entry.Exists() || entry.GetRef(&ref) != B_OK) {
			fprintf(stderr, "File %s not found\n", argv[i]);
			continue;
		}
		_OpenRef(ref);
	}
}


void
App::RefsReceived(BMessage* message)
{
	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
		_OpenRef(ref);
}


void
App::ReadyToRun()
{
	// Files from the command line or Tracker have opened by now; the
	// first new image doesn't ask for a size, as in the original.
	if (fWindowCount == 0)
		_NewImage(gSettings->BaseWidth(), gSettings->BaseHeight());
}


void
App::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_NEW:
		{
			int32 width = gSettings->BaseWidth();
			int32 height = gSettings->BaseHeight();
			if (!gSettings->AskCanvasSize()) {
				_NewImage(width, height);
				break;
			}
			// Suggest the size of the image on the clipboard, if any.
			ImageIO::ClipboardImageSize(width, height);
			(new SizeWindow(SIZE_NEW_IMAGE, width, height, BMessenger(this),
				NULL))->Show();
			break;
		}
		case MSG_SIZE_CHOSEN:
			if (message->GetInt32("mode", -1) == SIZE_NEW_IMAGE) {
				_NewImage(message->GetInt32("width", 1),
					message->GetInt32("height", 1));
			}
			break;

		case MSG_OPEN:
			if (fOpenPanel == NULL) {
				fOpenPanel = new BFilePanel(B_OPEN_PANEL, NULL, NULL,
					B_FILE_NODE, true, NULL, &fImageFilter);
				fOpenPanel->Window()->SetTitle(
					B_TRANSLATE("EasyPaint: Open image"));
			}
			fOpenPanel->Show();
			break;

		case MSG_SET_TOOL:
		{
			int32 tool = message->GetInt32("tool", TOOL_PEN);
			if (tool >= 0 && tool < TOOL_COUNT) {
				gSettings->SetTool((tool_id)tool);
				_Broadcast(MSG_STATE_CHANGED);
			}
			break;
		}
		case MSG_SET_COLOR:
		{
			rgb_color color;
			if (message->FindColor("color", &color) == B_OK) {
				gSettings->SetColor(message->GetInt32("which", 0), color);
				_Broadcast(MSG_STATE_CHANGED);
			}
			break;
		}
		case MSG_PEN_SIZE:
			gSettings->SetPenSize(message->GetInt32("size", 1));
			_Broadcast(MSG_STATE_CHANGED);
			break;

		case MSG_EDIT_COLOR:
		{
			int32 which = message->GetInt32("which", 0) == 0 ? 0 : 1;
			if (fColorWindows[which].IsValid()) {
				_Activate(fColorWindows[which]);
				break;
			}
			ColorWindow* window = new ColorWindow(which,
				message->GetPoint("where", BPoint(100, 100)));
			fColorWindows[which] = BMessenger(window);
			window->Show();
			break;
		}

		case MSG_SETTINGS:
			if (fSettingsWindow.IsValid()) {
				_Activate(fSettingsWindow);
				break;
			}
			{
				SettingsWindow* window = new SettingsWindow();
				fSettingsWindow = BMessenger(window);
				window->Show();
			}
			break;
		case MSG_SETTINGS_CHANGED:
			_Broadcast(MSG_SETTINGS_CHANGED);
			break;

		case MSG_WINDOW_CLOSED:
			if (--fWindowCount <= 0)
				PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BApplication::MessageReceived(message);
	}
}


void
App::AboutRequested()
{
	BString text;
	text.SetToFormat(B_TRANSLATE("EasyPaint %s\n\n"
		"A simple bitmap graphics editor.\n\n"
		"Written in Qt by the EasyPaint team (2012): Nikita Grishko, "
		"Artem Stepanyuk, Denis Klimenko and Bahdan Siamionau.\n"
		"Moved to Qt 5 for Haiku by Michel Clasquin-Johnson.\n"
		"Ported to Haiku's own interface in 2026.\n\n"
		"https://github.com/Gr1N/EasyPaint\n\n"
		"Distributed under the MIT license."), APP_VERSION);
	BAlert* alert = new BAlert(B_TRANSLATE("About EasyPaint"), text.String(),
		B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
	alert->Go(NULL);
}


bool
App::QuitRequested()
{
	if (!BApplication::QuitRequested())
		return false;
	gSettings->Save();
	return true;
}


void
App::_OpenRef(const entry_ref& ref)
{
	BBitmap* image = ImageIO::Load(ref);
	if (image == NULL) {
		BString text;
		text.SetToFormat(B_TRANSLATE("Can't open file \"%s\"."), ref.name);
		BAlert* alert = new BAlert(B_TRANSLATE("Error opening file"),
			text.String(), B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL,
			B_WARNING_ALERT);
		alert->Go(NULL);
		return;
	}
	_NewWindow(image, &ref);
}


void
App::_NewImage(int32 width, int32 height)
{
	BBitmap* image = ImageIO::NewImage(width, height);
	if (image == NULL) {
		BAlert* alert = new BAlert(B_TRANSLATE("New image"),
			B_TRANSLATE("There isn't enough memory for an image that size."),
			B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go(NULL);
		return;
	}
	_NewWindow(image, NULL);
}


void
App::_NewWindow(BBitmap* image, const entry_ref* ref)
{
	PaintWindow* window = new PaintWindow(_NextFrame(), image, ref);
	fWindowCount++;
	window->Show();
}


BRect
App::_NextFrame()
{
	BRect frame = gSettings->RestoreWindowFrame()
		? gSettings->WindowFrame() : BRect(100, 80, 900, 680);
	frame.OffsetBy(fCascade * 20, fCascade * 20);
	fCascade = (fCascade + 1) % 8;

	// Keep the window on the screen.
	BRect screen = BScreen().Frame();
	if (frame.Width() > screen.Width() - 20)
		frame.right = frame.left + screen.Width() - 20;
	if (frame.Height() > screen.Height() - 40)
		frame.bottom = frame.top + screen.Height() - 40;
	if (frame.right > screen.right)
		frame.OffsetBy(screen.right - frame.right - 10, 0);
	if (frame.bottom > screen.bottom)
		frame.OffsetBy(0, screen.bottom - frame.bottom - 10);
	if (frame.left < screen.left)
		frame.OffsetBy(screen.left - frame.left + 10, 0);
	if (frame.top < screen.top + 24)
		frame.OffsetBy(0, screen.top + 24 - frame.top);
	return frame;
}


void
App::_Broadcast(uint32 what)
{
	for (int32 i = 0; BWindow* window = WindowAt(i); i++) {
		if (dynamic_cast<PaintWindow*>(window) != NULL)
			window->PostMessage(what);
	}
}


void
App::_Activate(BMessenger& messenger)
{
	BLooper* looper;
	BWindow* window = dynamic_cast<BWindow*>(messenger.Target(&looper));
	if (window != NULL && window->Lock()) {
		window->Activate();
		window->Unlock();
	}
}


static void
PrintUsage()
{
	printf("EasyPaint - simple graphics painting program\n"
		"Usage: EasyPaint [options] [filename ...]\n\n"
		"Options:\n"
		"\t-h, --help\t\tshow this help message and exit\n"
		"\t-v, --version\t\tshow program's version number and exit\n");
}


int
main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			PrintUsage();
			return 0;
		}
		if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
			printf("%s\n", APP_VERSION);
			return 0;
		}
	}

	App app;
	app.Run();
	delete gSettings;
	return 0;
}
