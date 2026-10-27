/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * Asks for a size in pixels or as a percentage of the current one, for a
 * new image, scaling the image, or resizing the canvas. OK sends
 * MSG_SIZE_CHOSEN to the target.
 */
#ifndef SIZE_WINDOW_H
#define SIZE_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include "Constants.h"


class BCheckBox;
class BRadioButton;
class BStringView;
class NumberControl;


class SizeWindow : public BWindow {
public:
								SizeWindow(size_dialog_mode mode,
									int32 width, int32 height,
									BMessenger target, BWindow* parent);

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_UpdateEnabled();
			void				_SizeFromPercent();
			void				_UpdateLabel();

			size_dialog_mode	fMode;
			BMessenger			fTarget;
			int32				fOriginalWidth;
			int32				fOriginalHeight;
			int32				fWidth;
			int32				fHeight;
			int32				fWidthPercentValue;
			int32				fHeightPercentValue;

			BRadioButton*		fPixelsRadio;
			BRadioButton*		fPercentRadio;
			NumberControl*		fWidthControl;
			NumberControl*		fHeightControl;
			NumberControl*		fWidthPercent;
			NumberControl*		fHeightPercent;
			BCheckBox*			fKeepAspect;
			BStringView*		fNewSizeView;
};

#endif	// SIZE_WINDOW_H
