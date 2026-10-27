/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The text tool's floating window: the text to draw, and its font. Every
 * change is sent to the canvas at once, so the text shows on the image
 * as it's typed.
 */
#ifndef TEXT_WINDOW_H
#define TEXT_WINDOW_H

#include <Messenger.h>
#include <Window.h>


class BMenuField;
class BPopUpMenu;
class BTextView;
class NumberControl;


class TextWindow : public BWindow {
public:
								TextWindow(BMessenger canvas,
									const char* text, BWindow* owner);

	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage* message);

private:
			void				_BuildFontMenu();
			void				_MarkFont();
			void				_SendText();
			void				_Finish(bool cancel);

			BMessenger			fCanvas;
			BTextView*			fTextView;
			BPopUpMenu*			fFontMenu;
			BMenuField*			fFontField;
			NumberControl*		fSizeControl;
			bool				fFinished;
};

#endif	// TEXT_WINDOW_H
