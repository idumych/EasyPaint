/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "TextWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <TextView.h>

#include "Constants.h"
#include "NumberControl.h"
#include "Settings.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TextWindow"


enum {
	MSG_TEXT_EDITED	= 'tedt',
	MSG_FONT		= 'font',	// "family", "style": string
	MSG_FONT_SIZE	= 'fnsz',
	MSG_OK			= 'tok_',
	MSG_CANCEL		= 'tcnc'
};


// Tells its window about every edit.
class NotifyingTextView : public BTextView {
public:
	NotifyingTextView()
		:
		BTextView("text")
	{
	}

protected:
	virtual void InsertText(const char* text, int32 length, int32 offset,
		const text_run_array* runs)
	{
		BTextView::InsertText(text, length, offset, runs);
		_Notify();
	}

	virtual void DeleteText(int32 fromOffset, int32 toOffset)
	{
		BTextView::DeleteText(fromOffset, toOffset);
		_Notify();
	}

private:
	void _Notify()
	{
		if (Window() != NULL)
			Window()->PostMessage(MSG_TEXT_EDITED);
	}
};


TextWindow::TextWindow(BMessenger canvas, const char* text, BWindow* owner)
	:
	BWindow(BRect(0, 0, 320, 200), B_TRANSLATE("Text"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_SUBSET_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE),
	fCanvas(canvas),
	fFinished(false)
{
	if (owner != NULL)
		AddToSubset(owner);

	fTextView = new NotifyingTextView();
	fTextView->SetWordWrap(false);
	fTextView->SetText(text);
	fTextView->Select(fTextView->TextLength(), fTextView->TextLength());
	BScrollView* scrollView = new BScrollView("scroll", fTextView, 0, true,
		true);
	scrollView->SetExplicitMinSize(BSize(260, 100));

	fFontMenu = new BPopUpMenu("font");
	_BuildFontMenu();
	fFontField = new BMenuField("font", B_TRANSLATE("Font:"), fFontMenu);

	BFont font = gSettings->TextFont();
	fSizeControl = new NumberControl("size", B_TRANSLATE("Size:"), 4, 400,
		(int32)font.Size(), new BMessage(MSG_FONT_SIZE));

	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"),
		new BMessage(MSG_OK));
	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"),
		new BMessage(MSG_CANCEL));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(scrollView)
		.AddGroup(B_HORIZONTAL)
			.Add(fFontField)
			.Add(fSizeControl)
			.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
			.End();

	_MarkFont();
	fTextView->MakeFocus(true);

	if (owner != NULL) {
		BRect frame = owner->Frame();
		MoveTo(frame.right - Bounds().Width() - 40, frame.top + 80);
	} else
		CenterOnScreen();
}


bool
TextWindow::QuitRequested()
{
	// Closing the window keeps the text, like OK.
	_Finish(false);
	return true;
}


void
TextWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TEXT_EDITED:
			_SendText();
			break;

		case MSG_FONT:
		{
			const char* family;
			const char* style;
			if (message->FindString("family", &family) != B_OK
				|| message->FindString("style", &style) != B_OK) {
				break;
			}
			BFont font = gSettings->TextFont();
			font.SetFamilyAndStyle(family, style);
			gSettings->SetTextFont(font);
			_MarkFont();
			_SendText();
			break;
		}

		case MSG_FONT_SIZE:
		{
			BFont font = gSettings->TextFont();
			font.SetSize(fSizeControl->Number());
			gSettings->SetTextFont(font);
			_SendText();
			break;
		}

		case MSG_OK:
			_Finish(false);
			Quit();
			break;

		case MSG_CANCEL:
			_Finish(true);
			Quit();
			break;

		case MSG_TEXT_CLOSE:
			// The canvas finished the text itself.
			fFinished = true;
			Quit();
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
TextWindow::_BuildFontMenu()
{
	int32 familyCount = count_font_families();
	for (int32 i = 0; i < familyCount; i++) {
		font_family family;
		if (get_font_family(i, &family) != B_OK)
			continue;
		BMenu* styleMenu = new BMenu(family);
		int32 styleCount = count_font_styles(family);
		for (int32 j = 0; j < styleCount; j++) {
			font_style style;
			if (get_font_style(family, j, &style) != B_OK)
				continue;
			BMessage* message = new BMessage(MSG_FONT);
			message->AddString("family", family);
			message->AddString("style", style);
			styleMenu->AddItem(new BMenuItem(style, message));
		}
		BMenuItem* familyItem = new BMenuItem(styleMenu);
		fFontMenu->AddItem(familyItem);
	}
}


void
TextWindow::_MarkFont()
{
	BFont font = gSettings->TextFont();
	font_family family;
	font_style style;
	font.GetFamilyAndStyle(&family, &style);

	for (int32 i = 0; i < fFontMenu->CountItems(); i++) {
		BMenuItem* familyItem = fFontMenu->ItemAt(i);
		bool isFamily = strcmp(familyItem->Label(), family) == 0;
		familyItem->SetMarked(isFamily);
		BMenu* styleMenu = familyItem->Submenu();
		if (styleMenu == NULL)
			continue;
		for (int32 j = 0; j < styleMenu->CountItems(); j++) {
			BMenuItem* styleItem = styleMenu->ItemAt(j);
			styleItem->SetMarked(isFamily
				&& strcmp(styleItem->Label(), style) == 0);
		}
	}

	BString label;
	label.SetToFormat("%s %s", family, style);
	fFontField->MenuItem()->SetLabel(label.String());
}


void
TextWindow::_SendText()
{
	BMessage message(MSG_TEXT_CHANGED);
	message.AddString("text", fTextView->Text());
	fCanvas.SendMessage(&message);
}


void
TextWindow::_Finish(bool cancel)
{
	if (fFinished)
		return;
	fFinished = true;
	BMessage message(MSG_TEXT_DONE);
	message.AddBool("cancel", cancel);
	fCanvas.SendMessage(&message);
}
