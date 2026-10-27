/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The preferences. The original's language and keyboard shortcut pages
 * are gone: Haiku sets the language for all apps in the Locale
 * preferences, and its menus use the system's standard shortcuts.
 */
#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <Window.h>


class BCheckBox;
class NumberControl;


class SettingsWindow : public BWindow {
public:
								SettingsWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_UpdateEnabled();

			NumberControl*		fBaseWidth;
			NumberControl*		fBaseHeight;
			NumberControl*		fHistoryDepth;
			BCheckBox*			fAutoSave;
			NumberControl*		fAutoSaveInterval;
			BCheckBox*			fAskCanvasSize;
			BCheckBox*			fRestoreFrame;
};

#endif	// SETTINGS_WINDOW_H
