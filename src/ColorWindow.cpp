/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "ColorWindow.h"

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>

#include "Constants.h"
#include "Settings.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ColorWindow"


enum {
	MSG_COLOR_CHANGED	= 'clch',
	MSG_CANCEL			= 'clcn'
};


ColorWindow::ColorWindow(int32 which, BPoint where)
	:
	BWindow(BRect(0, 0, 300, 120), which == 0
			? B_TRANSLATE("Primary colour") : B_TRANSLATE("Secondary colour"),
		B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_CLOSE_ON_ESCAPE),
	fWhich(which)
{
	fOriginal = which == 0 ? gSettings->PrimaryColor()
		: gSettings->SecondaryColor();

	fControl = new BColorControl(B_ORIGIN, B_CELLS_32x8, 8, "color",
		new BMessage(MSG_COLOR_CHANGED), true);
	fControl->SetValue(fOriginal);

	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"),
		new BMessage(B_QUIT_REQUESTED));
	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"),
		new BMessage(MSG_CANCEL));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fControl)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
			.End();

	okButton->MakeDefault(true);
	MoveTo(where);
	MoveOnScreen();
}


void
ColorWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_COLOR_CHANGED:
			_SetColor(fControl->ValueAsColor());
			break;

		case MSG_CANCEL:
			_SetColor(fOriginal);
			Quit();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
ColorWindow::_SetColor(rgb_color color)
{
	BMessage setColor(MSG_SET_COLOR);
	setColor.AddInt32("which", fWhich);
	setColor.AddColor("color", color);
	be_app->PostMessage(&setColor);
}
