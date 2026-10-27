/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * Picks the primary or the secondary colour. Changes apply as they are
 * made; Cancel puts the old colour back.
 */
#ifndef COLOR_WINDOW_H
#define COLOR_WINDOW_H

#include <Window.h>


class BColorControl;


class ColorWindow : public BWindow {
public:
								ColorWindow(int32 which, BPoint where);

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_SetColor(rgb_color color);

			int32				fWhich;
			rgb_color			fOriginal;
			BColorControl*		fControl;
};

#endif	// COLOR_WINDOW_H
