/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * One image: the menu bar, the tool box, the canvas, the palette and a
 * status bar. The Qt original showed images as tabs in a single window;
 * here each image has its own window, as is usual on Haiku.
 */
#ifndef PAINT_WINDOW_H
#define PAINT_WINDOW_H

#include <Entry.h>
#include <Window.h>

#include "Constants.h"


class BBitmap;
class BFilePanel;
class BMenuBar;
class BMenuItem;
class BMessageRunner;
class BScrollView;
class BStringView;
class CanvasView;
class ColorSwatch;
class PaletteBar;
class ToolBox;


class PaintWindow : public BWindow {
public:
								// Takes ownership of image. ref may be
								// NULL for a new image.
								PaintWindow(BRect frame, BBitmap* image,
									const entry_ref* ref);
	virtual						~PaintWindow();

	virtual	bool				QuitRequested();
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				MenusBeginning();

private:
			void				_BuildMenuBar();
			BView*				_BuildStatusBar();

			void				_Save();
			void				_SaveAs();
			bool				_SaveTo(const entry_ref& directory,
									const char* name);
			void				_PageSetup();
			void				_Print();
			void				_OpenEffect(effect_id effect);

			void				_UpdateTitle();
			void				_UpdateMenus();
			void				_UpdateStatus();
			void				_UpdateState();
			void				_SwitchTool(tool_id tool);
			void				_StartAutoSave();

			CanvasView*			fCanvas;
			BScrollView*		fScrollView;
			ToolBox*			fToolBox;
			PaletteBar*			fPalette;
			BMenuBar*			fMenuBar;

			BStringView*		fSizeView;
			BStringView*		fPositionView;
			BStringView*		fZoomView;
			ColorSwatch*		fColorSwatch;
			BStringView*		fColorView;

			BMenuItem*			fUndoItem;
			BMenuItem*			fRedoItem;
			BMenuItem*			fCutItem;
			BMenuItem*			fCopyItem;
			BMenuItem*			fDeleteItem;
			BMenuItem*			fToolItems[TOOL_COUNT];

			entry_ref			fRef;
			bool				fHasRef;
			BFilePanel*			fSavePanel;
			bool				fQuitAfterSave;
			BMessageRunner*		fAutoSaveRunner;
			BMessage*			fPrintSettings;
			tool_id				fTool;
};

#endif	// PAINT_WINDOW_H
