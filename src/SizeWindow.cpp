/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "SizeWindow.h"

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <RadioButton.h>
#include <StringView.h>

#include <algorithm>

#include "NumberControl.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SizeWindow"


enum {
	MSG_UNITS_CHANGED	= 'unit',
	MSG_WIDTH			= 'wdth',
	MSG_HEIGHT			= 'hght',
	MSG_WIDTH_PERCENT	= 'wdpc',
	MSG_HEIGHT_PERCENT	= 'htpc',
	MSG_KEEP_ASPECT		= 'kasp',
	MSG_OK				= 'szok'
};


static const char*
TitleFor(size_dialog_mode mode)
{
	switch (mode) {
		case SIZE_NEW_IMAGE:
			return B_TRANSLATE("New image");
		case SIZE_IMAGE:
			return B_TRANSLATE("Image size");
		case SIZE_CANVAS:
		default:
			return B_TRANSLATE("Canvas size");
	}
}


static BString
SizeLabel(int32 width, int32 height)
{
	BString label;
	label.SetToFormat("%" B_PRId32 " × %" B_PRId32, width, height);
	return label;
}


SizeWindow::SizeWindow(size_dialog_mode mode, int32 width, int32 height,
	BMessenger target, BWindow* parent)
	:
	BWindow(BRect(0, 0, 300, 200), TitleFor(mode), B_TITLED_WINDOW_LOOK,
		parent != NULL ? B_MODAL_SUBSET_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_RESIZABLE | B_NOT_ZOOMABLE
			| B_CLOSE_ON_ESCAPE),
	fMode(mode),
	fTarget(target),
	fOriginalWidth(width),
	fOriginalHeight(height),
	fWidth(width),
	fHeight(height),
	fWidthPercentValue(100),
	fHeightPercentValue(100)
{
	if (parent != NULL)
		AddToSubset(parent);

	fNewSizeView = new BStringView("newSize", SizeLabel(width, height));

	fPixelsRadio = new BRadioButton("pixels", B_TRANSLATE("Pixels"),
		new BMessage(MSG_UNITS_CHANGED));
	fPixelsRadio->SetValue(B_CONTROL_ON);
	fPercentRadio = new BRadioButton("percent", B_TRANSLATE("Percent"),
		new BMessage(MSG_UNITS_CHANGED));

	fWidthControl = new NumberControl("width", B_TRANSLATE("Width:"), 1,
		9999, width, new BMessage(MSG_WIDTH));
	fHeightControl = new NumberControl("height", B_TRANSLATE("Height:"), 1,
		9999, height, new BMessage(MSG_HEIGHT));
	fWidthPercent = new NumberControl("widthPercent", B_TRANSLATE("Width:"),
		1, 1000, 100, new BMessage(MSG_WIDTH_PERCENT));
	fHeightPercent = new NumberControl("heightPercent",
		B_TRANSLATE("Height:"), 1, 1000, 100,
		new BMessage(MSG_HEIGHT_PERCENT));

	fKeepAspect = new BCheckBox("aspect", B_TRANSLATE("Keep aspect ratio"),
		new BMessage(MSG_KEEP_ASPECT));

	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"),
		new BMessage(MSG_OK));
	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"),
		new BMessage(B_QUIT_REQUESTED));

	BBox* sizeBox = new BBox("sizes");
	BGroupLayout* sizeLayout = BLayoutBuilder::Group<>(B_VERTICAL)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGrid()
			.Add(fPixelsRadio, 0, 0)
			.Add(fWidthControl, 1, 0)
			.Add(fHeightControl, 2, 0)
			.Add(fPercentRadio, 0, 1)
			.Add(fWidthPercent, 1, 1)
			.Add(fHeightPercent, 2, 1)
			.End()
		.Add(fKeepAspect);
	sizeBox->AddChild(sizeLayout->View());

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGrid()
			.Add(new BStringView("originalLabel",
				B_TRANSLATE("Original size:")), 0, 0)
			.Add(new BStringView("original", SizeLabel(width, height)), 1, 0)
			.Add(new BStringView("newLabel", B_TRANSLATE("New size:")), 0, 1)
			.Add(fNewSizeView, 1, 1)
			.AddGlue(2, 0)
			.End()
		.Add(sizeBox)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
			.End();

	okButton->MakeDefault(true);
	_UpdateEnabled();

	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();
}


void
SizeWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_UNITS_CHANGED:
			_UpdateEnabled();
			// The size follows whichever pair of fields is in use.
			if (fPixelsRadio->Value() == B_CONTROL_ON) {
				fWidth = fWidthControl->Number();
				fHeight = fHeightControl->Number();
			} else
				_SizeFromPercent();
			_UpdateLabel();
			break;

		// Setting a field's text sends its message again, so each case
		// only acts on a value that actually changed.
		case MSG_WIDTH:
		{
			int32 width = fWidthControl->Number();
			if (width == fWidth && !message->GetBool("force", false))
				break;
			fWidth = width;
			if (fKeepAspect->Value() == B_CONTROL_ON) {
				int32 height = std::max((int32)1,
					(int32)((int64)fOriginalHeight * fWidth / fOriginalWidth));
				if (height != fHeight) {
					fHeight = height;
					fHeightControl->SetNumber(height);
				}
			}
			_UpdateLabel();
			break;
		}

		case MSG_HEIGHT:
		{
			int32 height = fHeightControl->Number();
			if (height == fHeight)
				break;
			fHeight = height;
			if (fKeepAspect->Value() == B_CONTROL_ON) {
				int32 width = std::max((int32)1,
					(int32)((int64)fOriginalWidth * fHeight / fOriginalHeight));
				if (width != fWidth) {
					fWidth = width;
					fWidthControl->SetNumber(width);
				}
			}
			_UpdateLabel();
			break;
		}

		case MSG_WIDTH_PERCENT:
		{
			int32 percent = fWidthPercent->Number();
			if (percent == fWidthPercentValue
				&& !message->GetBool("force", false)) {
				break;
			}
			fWidthPercentValue = percent;
			if (fKeepAspect->Value() == B_CONTROL_ON
				&& fHeightPercentValue != percent) {
				fHeightPercentValue = percent;
				fHeightPercent->SetNumber(percent);
			}
			_SizeFromPercent();
			_UpdateLabel();
			break;
		}

		case MSG_HEIGHT_PERCENT:
		{
			int32 percent = fHeightPercent->Number();
			if (percent == fHeightPercentValue)
				break;
			fHeightPercentValue = percent;
			if (fKeepAspect->Value() == B_CONTROL_ON
				&& fWidthPercentValue != percent) {
				fWidthPercentValue = percent;
				fWidthPercent->SetNumber(percent);
			}
			_SizeFromPercent();
			_UpdateLabel();
			break;
		}

		case MSG_KEEP_ASPECT:
			if (fKeepAspect->Value() == B_CONTROL_ON) {
				// Make the other value follow the width.
				BMessage update(fPixelsRadio->Value() == B_CONTROL_ON
					? MSG_WIDTH : MSG_WIDTH_PERCENT);
				update.AddBool("force", true);
				PostMessage(&update);
			}
			break;

		case MSG_OK:
		{
			BMessage reply(MSG_SIZE_CHOSEN);
			reply.AddInt32("mode", fMode);
			reply.AddInt32("width", fWidth);
			reply.AddInt32("height", fHeight);
			fTarget.SendMessage(&reply);
			Quit();
			break;
		}

		default:
			BWindow::MessageReceived(message);
	}
}


void
SizeWindow::_UpdateEnabled()
{
	bool pixels = fPixelsRadio->Value() == B_CONTROL_ON;
	fWidthControl->SetEnabled(pixels);
	fHeightControl->SetEnabled(pixels);
	fWidthPercent->SetEnabled(!pixels);
	fHeightPercent->SetEnabled(!pixels);
}


void
SizeWindow::_SizeFromPercent()
{
	fWidth = std::max((int32)1,
		(int32)((int64)fOriginalWidth * fWidthPercentValue / 100));
	fHeight = std::max((int32)1,
		(int32)((int64)fOriginalHeight * fHeightPercentValue / 100));
}


void
SizeWindow::_UpdateLabel()
{
	fNewSizeView->SetText(SizeLabel(fWidth, fHeight));
}
