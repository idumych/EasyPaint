/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The image being edited, shown at a zoom factor inside a scroll view.
 * It owns the image bitmap, the undo history and one instance of every
 * tool, and passes mouse input on to the current tool. A small grip at
 * the image's bottom-right corner resizes the canvas.
 *
 * Unlike the Qt original, zooming only changes the view: the image keeps
 * its pixels.
 */
#ifndef CANVAS_VIEW_H
#define CANVAS_VIEW_H

#include <View.h>

#include <deque>

#include "Constants.h"
#include "ImageOps.h"


class BBitmap;
class Tool;
class SelectionTool;
class TextTool;


class CanvasView : public BView {
public:
								CanvasView(BBitmap* image);
	virtual						~CanvasView();

	// The image
			BBitmap*			Image() const { return fImage; }
			int32				ImageWidth() const;
			int32				ImageHeight() const;
			BRect				ImageBounds() const;
			ImageOps::Pixels	Pixels() const;
			BBitmap*			CopyImage() const;
			// Takes ownership. The old image goes on the undo history if
			// pushUndo is set.
			void				ReplaceImage(BBitmap* image, bool pushUndo);

			bool				IsModified() const { return fModified; }
			// True while a tool is in the middle of something (a drag, a
			// selection, text being typed).
			bool				IsBusy() const;
			void				SetModified(bool modified);

	// Drawing into the image, for the tools. BeginDrawing() locks the
	// image's off-screen view and resets its drawing state; EndDrawing()
	// unlocks it. The tool then calls InvalidateImage() for what changed.
			BView*				BeginDrawing();
			void				EndDrawing();
			// Saves the image so a tool can restore it before redrawing a
			// shape it's still dragging out.
			void				SavePreviewBase();
			void				RestorePreviewBase();
			void				DropPreviewBase();

	// Undo history
			void				PushUndo();
			void				DropLastUndo();
			void				Undo();
			void				Redo();
			bool				CanUndo() const { return !fUndo.empty(); }
			bool				CanRedo() const { return !fRedo.empty(); }

	// Selection and clipboard
			bool				HasSelection() const;
			void				Cut();
			void				Copy();
			void				Paste();
			void				SelectAll();
			void				DeleteSelection();
			// Commits whatever a tool is in the middle of (a floating
			// selection, text, a curve): before saving, effects, resizing.
			void				FinishTools();
			void				ToolChanged();
			void				SetText(const char* text);
			void				TextDone(bool cancel);

	// Image menu and effects
			void				ApplySimpleEffect(effect_id effect);
			void				ResizeImage(int32 width, int32 height);
			void				ResizeCanvas(int32 width, int32 height);
			void				Rotate(bool clockwise);

	// Zoom
			float				Zoom() const { return fZoom; }
			void				SetZoom(float zoom, BPoint keepAt
									= BPoint(-1, -1));
			void				ZoomIn(BPoint keepAt = BPoint(-1, -1));
			void				ZoomOut(BPoint keepAt = BPoint(-1, -1));

	// Coordinates
			BPoint				ToImage(BPoint viewPoint) const;
			BRect				ToView(BRect imageRect) const;
			BPoint				ToView(BPoint imagePoint) const;
			bool				PickColor(BPoint imagePoint,
									rgb_color& color) const;

	// Called by the tools
			void				InvalidateImage(BRect imageRect);
			void				NotifyChanged();
			void				UpdateCursor();
			BPoint				LastPointer() const { return fLastPointer; }

	virtual	void				AttachedToWindow();
	virtual	void				Draw(BRect updateRect);
	virtual	void				FrameResized(float width, float height);
	virtual	void				KeyDown(const char* bytes, int32 numBytes);
	virtual	void				MessageReceived(BMessage* message);
	virtual	void				MouseDown(BPoint where);
	virtual	void				MouseMoved(BPoint where, uint32 transit,
									const BMessage* dragMessage);
	virtual	void				MouseUp(BPoint where);

private:
			Tool*				_CurrentTool() const;
			bool				_InCanvasGrip(BPoint viewPoint) const;
			void				_UpdateScrollBars();
			void				_SetCursorFor(BPoint viewPoint);
			void				_TrimHistory(std::deque<BBitmap*>& history);
			void				_ClearHistory(std::deque<BBitmap*>& history);
			void				_ImageSizeChanged();
			void				_DrawCheckerboard(BRect rect);

			BBitmap*			fImage;
			BView*				fImageView;
			BBitmap*			fPreviewBase;
			std::deque<BBitmap*> fUndo;
			std::deque<BBitmap*> fRedo;

			Tool*				fTools[TOOL_COUNT];
			SelectionTool*		fSelectionTool;
			TextTool*			fTextTool;
			Tool*				fActiveTool;	// the tool that got MouseDown

			float				fZoom;
			bool				fModified;
			uint32				fButtons;

			bool				fResizingCanvas;
			BBitmap*			fResizeOriginal;

			BPoint				fLastPointer;
};


#endif	// CANVAS_VIEW_H
