/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The state every window shares: the current tool, colours, pen size and
 * text font, plus the preferences from the Settings window. All windows
 * read it from their own threads, so every accessor takes the lock.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <Font.h>
#include <GraphicsDefs.h>
#include <Locker.h>
#include <Rect.h>

#include "Constants.h"


class Settings {
public:
								Settings();

			status_t			Load();
			status_t			Save();

			// Drawing state (not saved, as in the original)
			rgb_color			PrimaryColor();
			rgb_color			SecondaryColor();
			void				SetColor(int32 which, rgb_color color);
			int32				PenSize();
			void				SetPenSize(int32 size);
			tool_id				Tool();
			tool_id				PreviousTool();
			void				SetTool(tool_id tool);
			BFont				TextFont();
			void				SetTextFont(const BFont& font);

			// Preferences
			int32				BaseWidth();
			int32				BaseHeight();
			void				SetBaseSize(int32 width, int32 height);
			int32				HistoryDepth();
			void				SetHistoryDepth(int32 depth);
			bool				AutoSave();
			int32				AutoSaveInterval();
			void				SetAutoSave(bool enabled, int32 seconds);
			bool				AskCanvasSize();
			void				SetAskCanvasSize(bool ask);
			bool				RestoreWindowFrame();
			void				SetRestoreWindowFrame(bool restore);
			BRect				WindowFrame();
			void				SetWindowFrame(BRect frame);

private:
			BLocker				fLock;

			rgb_color			fPrimary;
			rgb_color			fSecondary;
			int32				fPenSize;
			tool_id				fTool;
			tool_id				fPreviousTool;
			BFont				fTextFont;

			int32				fBaseWidth;
			int32				fBaseHeight;
			int32				fHistoryDepth;
			bool				fAutoSave;
			int32				fAutoSaveInterval;
			bool				fAskCanvasSize;
			bool				fRestoreWindowFrame;
			BRect				fWindowFrame;
};


extern Settings* gSettings;

#endif	// SETTINGS_H
