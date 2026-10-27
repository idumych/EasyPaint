/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The drawing tools. Each canvas has one of each; the canvas gives the
 * current one the mouse events, with the point already converted to
 * image coordinates (whole pixels). As in the original, the primary mouse
 * button draws in the primary colour and the secondary button in the
 * secondary colour; rectangles and ellipses drawn with the secondary
 * button are filled with the secondary colour.
 */
#ifndef TOOLS_H
#define TOOLS_H

#include <GraphicsDefs.h>
#include <Messenger.h>
#include <Point.h>
#include <Rect.h>
#include <String.h>


class BBitmap;
class BCursor;
class BView;
class CanvasView;


class Tool {
public:
								Tool(CanvasView* canvas);
	virtual						~Tool();

	virtual	void				MouseDown(BPoint point, uint32 buttons) = 0;
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);

	// Commits anything in progress (the tool is being switched away
	// from, or the image is about to change underneath it).
	virtual	void				Finish();
	// Draws on top of the image, in the canvas's coordinates.
	virtual	void				DrawOverlay(BView* view);
	virtual	const BCursor*		Cursor(BPoint viewPoint);

protected:
	static	bool				IsDrawingButton(uint32 buttons);
	static	rgb_color			ColorFor(uint32 buttons);
	static	void				SetUpPen(BView* view, rgb_color color,
									float size);
	static	void				DrawDot(BView* view, BPoint point,
									float size);
			BRect				StrokeBounds(BPoint a, BPoint b,
									float size) const;

			CanvasView*			fCanvas;
};


class PenTool : public Tool {
public:
								PenTool(CanvasView* canvas, bool eraser);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	const BCursor*		Cursor(BPoint viewPoint);

private:
			void				_Stroke(BPoint from, BPoint to);

			bool				fEraser;
			bool				fDrawing;
			uint32				fButtons;
			BPoint				fLast;
};


class SprayTool : public Tool {
public:
								SprayTool(CanvasView* canvas);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	const BCursor*		Cursor(BPoint viewPoint);

private:
			void				_Spray(BPoint point);

			bool				fDrawing;
			uint32				fButtons;
};


// Line, rectangle and ellipse: dragged out from a corner, redrawn over a
// saved copy of the image while the mouse moves.
class ShapeTool : public Tool {
public:
	enum shape {
		SHAPE_LINE,
		SHAPE_RECTANGLE,
		SHAPE_ELLIPSE
	};
								ShapeTool(CanvasView* canvas, shape kind);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	void				Finish();

private:
			void				_Redraw();

			shape				fShape;
			bool				fDrawing;
			uint32				fButtons;
			BPoint				fStart;
			BPoint				fEnd;
			BRect				fDrawn;
};


// A cubic Bezier curve: drag out the end points, then click or drag
// twice more to place the two control points.
class CurveTool : public Tool {
public:
								CurveTool(CanvasView* canvas);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	void				Finish();

private:
			void				_Redraw();

			int32				fStep;		// 0 idle, 1 end points, 2-3 controls
			bool				fDrawing;
			uint32				fButtons;
			BPoint				fPoints[4];	// start, control 1, 2, end
};


class FillTool : public Tool {
public:
								FillTool(CanvasView* canvas);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	const BCursor*		Cursor(BPoint viewPoint);
};


class ColorPickerTool : public Tool {
public:
								ColorPickerTool(CanvasView* canvas);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	const BCursor*		Cursor(BPoint viewPoint);

private:
			uint32				fButtons;
};


class MagnifierTool : public Tool {
public:
								MagnifierTool(CanvasView* canvas);

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	const BCursor*		Cursor(BPoint viewPoint);
};


// A rectangle the user drags out and can move afterwards; the base of the
// selection and text tools.
class MarqueeTool : public Tool {
public:
								MarqueeTool(CanvasView* canvas);

	virtual	void				DrawOverlay(BView* view);

protected:
			bool				_InRect(BPoint point) const;
			bool				_InGrip(BPoint viewPoint) const;
			BRect				_Rect() const;
			void				_StartDrag(BPoint point);
			void				_DragTo(BPoint point);
			void				_Invalidate();

			bool				fHasRect;
			int32				fX;
			int32				fY;
			int32				fWidth;
			int32				fHeight;
			BPoint				fAnchor;
};


class SelectionTool : public MarqueeTool {
public:
								SelectionTool(CanvasView* canvas);
	virtual						~SelectionTool();

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	void				Finish();
	virtual	void				DrawOverlay(BView* view);
	virtual	const BCursor*		Cursor(BPoint viewPoint);

			bool				HasSelection() const { return fHasRect; }
			void				Copy();
			void				Delete();
			void				Paste(BBitmap* bitmap, BPoint where);
			void				SelectAll();

private:
	enum state {
		STATE_IDLE,
		STATE_SELECTING,
		STATE_MOVING,
		STATE_RESIZING
	};

			void				_Lift();
			void				_Commit();
			BBitmap*			_SelectedBitmap() const;
			void				_ShowMenu();

			state				fState;
			BBitmap*			fFloating;
			BPoint				fGrabOffset;
};


class TextTool : public MarqueeTool {
public:
								TextTool(CanvasView* canvas);
	virtual						~TextTool();

	virtual	void				MouseDown(BPoint point, uint32 buttons);
	virtual	void				MouseMoved(BPoint point, uint32 buttons);
	virtual	void				MouseUp(BPoint point, uint32 buttons);
	virtual	void				Finish();
	virtual	const BCursor*		Cursor(BPoint viewPoint);

			void				SetText(const char* text);
			void				Done(bool cancel);
			bool				IsEditing() const { return fHasRect; }

private:
			void				_Render();
			void				_OpenWindow();
			void				_CloseWindow();

			bool				fSelecting;
			bool				fMoving;
			bool				fUndoPushed;
			BPoint				fGrabOffset;
			BString				fText;
			BMessenger			fWindow;
};

#endif	// TOOLS_H
