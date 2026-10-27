/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The panels around the canvas: the tool box on the left (tools, the
 * primary and secondary colours, the pen size) and the colour palette
 * below. They send their changes to the application, which shares them
 * with every window, and Update() shows the current state.
 */
#ifndef TOOL_BOX_H
#define TOOL_BOX_H

#include <GroupView.h>

#include "Constants.h"


class BButton;
class NumberControl;
class StepperButton;


class ColorSwatch : public BView {
public:
	enum kind {
		SWATCH_PRIMARY,		// click to edit, accepts dropped colours
		SWATCH_SECONDARY,
		SWATCH_PALETTE,		// primary button: primary, secondary: second.
		SWATCH_DISPLAY		// just shows a colour
	};

								ColorSwatch(kind type, rgb_color color,
									float size);

			void				SetColor(rgb_color color);
			rgb_color			Color() const { return fColor; }

	virtual	void				Draw(BRect updateRect);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MessageReceived(BMessage* message);

private:
			kind				fKind;
			rgb_color			fColor;
};


class ToolBox : public BGroupView {
public:
								ToolBox();

	static	const char*			ToolLabel(tool_id tool);
	static	const char*			ToolIconName(tool_id tool);

	virtual	void				AttachedToWindow();
	virtual	void				MessageReceived(BMessage* message);
			void				Update();

private:
			BButton*			fButtons[TOOL_COUNT];
			ColorSwatch*		fPrimary;
			ColorSwatch*		fSecondary;
			NumberControl*		fPenField;
			StepperButton*		fPenStepper;
};


class PaletteBar : public BGroupView {
public:
								PaletteBar();
};

#endif	// TOOL_BOX_H
