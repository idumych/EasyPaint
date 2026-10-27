/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "Settings.h"

#include <Autolock.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>


Settings* gSettings = NULL;

static const char* kSettingsFile = "EasyPaint settings";


static status_t
SettingsPath(BPath& path)
{
	status_t status = find_directory(B_USER_SETTINGS_DIRECTORY, &path);
	if (status == B_OK)
		status = path.Append(kSettingsFile);
	return status;
}


Settings::Settings()
	:
	fLock("settings"),
	fPenSize(1),
	fTool(TOOL_PEN),
	fPreviousTool(TOOL_PEN),
	fTextFont(be_plain_font),
	fBaseWidth(400),
	fBaseHeight(300),
	fHistoryDepth(40),
	fAutoSave(false),
	fAutoSaveInterval(300),
	fAskCanvasSize(true),
	fRestoreWindowFrame(true),
	fWindowFrame(100, 80, 900, 680)
{
	fPrimary = make_color(0, 0, 0);
	fSecondary = make_color(255, 255, 255);
	fTextFont.SetSize(12);
}


status_t
Settings::Load()
{
	BPath path;
	status_t status = SettingsPath(path);
	if (status != B_OK)
		return status;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage archive;
	status = archive.Unflatten(&file);
	if (status != B_OK)
		return status;

	BAutolock _(fLock);
	int32 width, height, value;
	bool flag;
	BRect frame;
	if (archive.FindInt32("base width", &width) == B_OK
		&& archive.FindInt32("base height", &height) == B_OK
		&& width > 0 && height > 0) {
		fBaseWidth = width;
		fBaseHeight = height;
	}
	if (archive.FindInt32("history depth", &value) == B_OK && value > 0)
		fHistoryDepth = value;
	if (archive.FindBool("autosave", &flag) == B_OK)
		fAutoSave = flag;
	if (archive.FindInt32("autosave interval", &value) == B_OK && value > 0)
		fAutoSaveInterval = value;
	if (archive.FindBool("ask canvas size", &flag) == B_OK)
		fAskCanvasSize = flag;
	if (archive.FindBool("restore window frame", &flag) == B_OK)
		fRestoreWindowFrame = flag;
	if (archive.FindRect("window frame", &frame) == B_OK && frame.IsValid())
		fWindowFrame = frame;

	const char* family;
	const char* style;
	float size;
	if (archive.FindString("font family", &family) == B_OK
		&& archive.FindString("font style", &style) == B_OK
		&& archive.FindFloat("font size", &size) == B_OK) {
		fTextFont.SetFamilyAndStyle(family, style);
		fTextFont.SetSize(size);
	}
	return B_OK;
}


status_t
Settings::Save()
{
	BMessage archive;
	{
		BAutolock _(fLock);
		archive.AddInt32("base width", fBaseWidth);
		archive.AddInt32("base height", fBaseHeight);
		archive.AddInt32("history depth", fHistoryDepth);
		archive.AddBool("autosave", fAutoSave);
		archive.AddInt32("autosave interval", fAutoSaveInterval);
		archive.AddBool("ask canvas size", fAskCanvasSize);
		archive.AddBool("restore window frame", fRestoreWindowFrame);
		archive.AddRect("window frame", fWindowFrame);

		font_family family;
		font_style style;
		fTextFont.GetFamilyAndStyle(&family, &style);
		archive.AddString("font family", family);
		archive.AddString("font style", style);
		archive.AddFloat("font size", fTextFont.Size());
	}

	BPath path;
	status_t status = SettingsPath(path);
	if (status != B_OK)
		return status;
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status = file.InitCheck();
	if (status != B_OK)
		return status;
	return archive.Flatten(&file);
}


rgb_color
Settings::PrimaryColor()
{
	BAutolock _(fLock);
	return fPrimary;
}


rgb_color
Settings::SecondaryColor()
{
	BAutolock _(fLock);
	return fSecondary;
}


void
Settings::SetColor(int32 which, rgb_color color)
{
	BAutolock _(fLock);
	color.alpha = 255;
	if (which == 0)
		fPrimary = color;
	else
		fSecondary = color;
}


int32
Settings::PenSize()
{
	BAutolock _(fLock);
	return fPenSize;
}


void
Settings::SetPenSize(int32 size)
{
	BAutolock _(fLock);
	fPenSize = size;
}


tool_id
Settings::Tool()
{
	BAutolock _(fLock);
	return fTool;
}


tool_id
Settings::PreviousTool()
{
	BAutolock _(fLock);
	return fPreviousTool;
}


void
Settings::SetTool(tool_id tool)
{
	BAutolock _(fLock);
	// The colour picker hands back to the tool that was in use before it.
	if (tool != fTool && fTool != TOOL_COLOR_PICKER)
		fPreviousTool = fTool;
	fTool = tool;
}


BFont
Settings::TextFont()
{
	BAutolock _(fLock);
	return fTextFont;
}


void
Settings::SetTextFont(const BFont& font)
{
	BAutolock _(fLock);
	fTextFont = font;
}


int32
Settings::BaseWidth()
{
	BAutolock _(fLock);
	return fBaseWidth;
}


int32
Settings::BaseHeight()
{
	BAutolock _(fLock);
	return fBaseHeight;
}


void
Settings::SetBaseSize(int32 width, int32 height)
{
	BAutolock _(fLock);
	fBaseWidth = width;
	fBaseHeight = height;
}


int32
Settings::HistoryDepth()
{
	BAutolock _(fLock);
	return fHistoryDepth;
}


void
Settings::SetHistoryDepth(int32 depth)
{
	BAutolock _(fLock);
	fHistoryDepth = depth;
}


bool
Settings::AutoSave()
{
	BAutolock _(fLock);
	return fAutoSave;
}


int32
Settings::AutoSaveInterval()
{
	BAutolock _(fLock);
	return fAutoSaveInterval;
}


void
Settings::SetAutoSave(bool enabled, int32 seconds)
{
	BAutolock _(fLock);
	fAutoSave = enabled;
	fAutoSaveInterval = seconds;
}


bool
Settings::AskCanvasSize()
{
	BAutolock _(fLock);
	return fAskCanvasSize;
}


void
Settings::SetAskCanvasSize(bool ask)
{
	BAutolock _(fLock);
	fAskCanvasSize = ask;
}


bool
Settings::RestoreWindowFrame()
{
	BAutolock _(fLock);
	return fRestoreWindowFrame;
}


void
Settings::SetRestoreWindowFrame(bool restore)
{
	BAutolock _(fLock);
	fRestoreWindowFrame = restore;
}


BRect
Settings::WindowFrame()
{
	BAutolock _(fLock);
	return fWindowFrame;
}


void
Settings::SetWindowFrame(BRect frame)
{
	BAutolock _(fLock);
	fWindowFrame = frame;
}
