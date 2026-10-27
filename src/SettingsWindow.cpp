/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "SettingsWindow.h"

#include <Application.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <StringView.h>

#include "Constants.h"
#include "NumberControl.h"
#include "Settings.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsWindow"


enum {
	MSG_AUTOSAVE_TOGGLED	= 'astg',
	MSG_OK					= 'stok'
};


SettingsWindow::SettingsWindow()
	:
	BWindow(BRect(0, 0, 360, 260), B_TRANSLATE("Settings"),
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_CLOSE_ON_ESCAPE)
{
	BMessage noop('noop');
	fBaseWidth = new NumberControl("width", B_TRANSLATE("Width:"), 1, 9999,
		gSettings->BaseWidth(), new BMessage(noop));
	fBaseHeight = new NumberControl("height", B_TRANSLATE("Height:"), 1,
		9999, gSettings->BaseHeight(), new BMessage(noop));
	fHistoryDepth = new NumberControl("history", B_TRANSLATE("Undo steps:"),
		1, 99, gSettings->HistoryDepth(), new BMessage(noop));
	fAutoSave = new BCheckBox("autosave", B_TRANSLATE("Save automatically"),
		new BMessage(MSG_AUTOSAVE_TOGGLED));
	fAutoSave->SetValue(gSettings->AutoSave());
	fAutoSaveInterval = new NumberControl("interval",
		B_TRANSLATE("Every (seconds):"), 1, 3000,
		gSettings->AutoSaveInterval(), new BMessage(noop));

	fAskCanvasSize = new BCheckBox("askSize",
		B_TRANSLATE("Ask for the size of new images"), NULL);
	fAskCanvasSize->SetValue(gSettings->AskCanvasSize());
	fRestoreFrame = new BCheckBox("restoreFrame",
		B_TRANSLATE("Open windows where the last one was closed"), NULL);
	fRestoreFrame->SetValue(gSettings->RestoreWindowFrame());

	BBox* imageBox = new BBox("image");
	imageBox->SetLabel(B_TRANSLATE("Image"));
	BGroupLayout* imageLayout = BLayoutBuilder::Group<>(B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(new BStringView("base", B_TRANSLATE("New image size:")))
			.Add(fBaseWidth)
			.Add(fBaseHeight)
			.AddGlue()
			.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fHistoryDepth)
			.AddGlue()
			.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fAutoSave)
			.Add(fAutoSaveInterval)
			.AddGlue()
			.End();
	imageBox->AddChild(imageLayout->View());

	BBox* windowBox = new BBox("window");
	windowBox->SetLabel(B_TRANSLATE("Windows"));
	BGroupLayout* windowLayout = BLayoutBuilder::Group<>(B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.Add(fAskCanvasSize)
		.Add(fRestoreFrame);
	windowBox->AddChild(windowLayout->View());

	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"),
		new BMessage(MSG_OK));
	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"),
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(imageBox)
		.Add(windowBox)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
			.End();

	okButton->MakeDefault(true);
	_UpdateEnabled();
	CenterOnScreen();
}


void
SettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_AUTOSAVE_TOGGLED:
			_UpdateEnabled();
			break;

		case MSG_OK:
			gSettings->SetBaseSize(fBaseWidth->Number(), fBaseHeight->Number());
			gSettings->SetHistoryDepth(fHistoryDepth->Number());
			gSettings->SetAutoSave(fAutoSave->Value() == B_CONTROL_ON,
				fAutoSaveInterval->Number());
			gSettings->SetAskCanvasSize(
				fAskCanvasSize->Value() == B_CONTROL_ON);
			gSettings->SetRestoreWindowFrame(
				fRestoreFrame->Value() == B_CONTROL_ON);
			gSettings->Save();
			be_app->PostMessage(MSG_SETTINGS_CHANGED);
			Quit();
			break;

		case 'noop':
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
SettingsWindow::_UpdateEnabled()
{
	fAutoSaveInterval->SetEnabled(fAutoSave->Value() == B_CONTROL_ON);
}
