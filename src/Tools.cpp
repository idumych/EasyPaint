/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "Tools.h"

#include <Application.h>
#include <Bitmap.h>
#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <View.h>
#include <Window.h>

#include <math.h>
#include <stdlib.h>

#include <algorithm>

#include "CanvasView.h"
#include "Constants.h"
#include "Cursors.h"
#include "ImageIO.h"
#include "ImageOps.h"
#include "Settings.h"
#include "TextWindow.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Tools"


static const float kGripSize = 6;

// Diagonal stripes, for the "marching ants" frame of a selection.
static const pattern kStripes = {
	{ 0xf0, 0xe1, 0xc3, 0x87, 0x0f, 0x1e, 0x3c, 0x78 }
};


static ImageOps::Pixels
PixelsOf(const BBitmap* bitmap)
{
	ImageOps::Pixels pixels;
	pixels.bits = (uint32_t*)bitmap->Bits();
	pixels.width = bitmap->Bounds().IntegerWidth() + 1;
	pixels.height = bitmap->Bounds().IntegerHeight() + 1;
	pixels.stride = bitmap->BytesPerRow() / 4;
	return pixels;
}


static uint32
PixelOf(rgb_color color)
{
	return ImageOps::MakePixel(color.red, color.green, color.blue,
		color.alpha);
}


// #pragma mark - Tool


Tool::Tool(CanvasView* canvas)
	:
	fCanvas(canvas)
{
}


Tool::~Tool()
{
}


void
Tool::MouseMoved(BPoint point, uint32 buttons)
{
}


void
Tool::MouseUp(BPoint point, uint32 buttons)
{
}


void
Tool::Finish()
{
}


void
Tool::DrawOverlay(BView* view)
{
}


const BCursor*
Tool::Cursor(BPoint viewPoint)
{
	return Cursors::System(B_CURSOR_ID_CROSS_HAIR);
}


bool
Tool::IsDrawingButton(uint32 buttons)
{
	return (buttons & (B_PRIMARY_MOUSE_BUTTON | B_SECONDARY_MOUSE_BUTTON))
		!= 0;
}


rgb_color
Tool::ColorFor(uint32 buttons)
{
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0)
		return gSettings->SecondaryColor();
	return gSettings->PrimaryColor();
}


void
Tool::SetUpPen(BView* view, rgb_color color, float size)
{
	view->SetHighColor(color);
	view->SetPenSize(size);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
}


void
Tool::DrawDot(BView* view, BPoint point, float size)
{
	if (size <= 1.5f)
		view->FillRect(BRect(point, point));
	else
		view->FillEllipse(point, size / 2, size / 2);
}


BRect
Tool::StrokeBounds(BPoint a, BPoint b, float size) const
{
	BRect bounds(std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x),
		std::max(a.y, b.y));
	float inset = ceilf(size / 2) + 1;
	return bounds.InsetByCopy(-inset, -inset);
}


// #pragma mark - PenTool


PenTool::PenTool(CanvasView* canvas, bool eraser)
	:
	Tool(canvas),
	fEraser(eraser),
	fDrawing(false),
	fButtons(0)
{
}


void
PenTool::MouseDown(BPoint point, uint32 buttons)
{
	if (!IsDrawingButton(buttons))
		return;
	fCanvas->PushUndo();
	fDrawing = true;
	fButtons = buttons;
	fLast = point;
	_Stroke(point, point);
}


void
PenTool::MouseMoved(BPoint point, uint32 buttons)
{
	if (!fDrawing || point == fLast)
		return;
	_Stroke(fLast, point);
	fLast = point;
}


void
PenTool::MouseUp(BPoint point, uint32 buttons)
{
	fDrawing = false;
}


const BCursor*
PenTool::Cursor(BPoint viewPoint)
{
	rgb_color fill = fEraser ? kBackgroundWhite : gSettings->PrimaryColor();
	return Cursors::Brush(gSettings->PenSize() * fCanvas->Zoom(), fill);
}


void
PenTool::_Stroke(BPoint from, BPoint to)
{
	float size = gSettings->PenSize();
	BView* view = fCanvas->BeginDrawing();
	SetUpPen(view, fEraser ? kBackgroundWhite : ColorFor(fButtons), size);
	if (from == to)
		DrawDot(view, to, size);
	else
		view->StrokeLine(from, to);
	fCanvas->EndDrawing();
	fCanvas->InvalidateImage(StrokeBounds(from, to, size));
}


// #pragma mark - SprayTool


static int
RandomBelow(int bound)
{
	return rand() % bound;
}


SprayTool::SprayTool(CanvasView* canvas)
	:
	Tool(canvas),
	fDrawing(false),
	fButtons(0)
{
}


void
SprayTool::MouseDown(BPoint point, uint32 buttons)
{
	if (!IsDrawingButton(buttons))
		return;
	fCanvas->PushUndo();
	fDrawing = true;
	fButtons = buttons;
	_Spray(point);
}


void
SprayTool::MouseMoved(BPoint point, uint32 buttons)
{
	if (fDrawing)
		_Spray(point);
}


void
SprayTool::MouseUp(BPoint point, uint32 buttons)
{
	fDrawing = false;
}


const BCursor*
SprayTool::Cursor(BPoint viewPoint)
{
	return Cursors::FromResource("cursor_spray");
}


void
SprayTool::_Spray(BPoint point)
{
	float size = gSettings->PenSize();
	ImageOps::SprayDot dots[12];
	ImageOps::SprayPattern(size, RandomBelow, dots);

	float dotSize = sqrtf(size);
	BView* view = fCanvas->BeginDrawing();
	SetUpPen(view, ColorFor(fButtons), dotSize);
	BRect dirty(point, point);
	for (const ImageOps::SprayDot& dot : dots) {
		BPoint where(point.x + dot.x, point.y + dot.y);
		DrawDot(view, where, dotSize);
		dirty = dirty | BRect(where, where);
	}
	fCanvas->EndDrawing();
	fCanvas->InvalidateImage(dirty.InsetByCopy(-dotSize, -dotSize));
}


// #pragma mark - ShapeTool


ShapeTool::ShapeTool(CanvasView* canvas, shape kind)
	:
	Tool(canvas),
	fShape(kind),
	fDrawing(false),
	fButtons(0)
{
}


void
ShapeTool::MouseDown(BPoint point, uint32 buttons)
{
	if (!IsDrawingButton(buttons))
		return;
	fCanvas->PushUndo();
	fCanvas->SavePreviewBase();
	fDrawing = true;
	fButtons = buttons;
	fStart = fEnd = point;
	fDrawn = BRect();
	_Redraw();
}


void
ShapeTool::MouseMoved(BPoint point, uint32 buttons)
{
	if (!fDrawing || point == fEnd)
		return;
	fEnd = point;
	_Redraw();
}


void
ShapeTool::MouseUp(BPoint point, uint32 buttons)
{
	if (!fDrawing)
		return;
	fEnd = point;
	_Redraw();
	Finish();
}


void
ShapeTool::Finish()
{
	if (!fDrawing)
		return;
	fDrawing = false;
	fCanvas->DropPreviewBase();
}


void
ShapeTool::_Redraw()
{
	fCanvas->RestorePreviewBase();

	float size = gSettings->PenSize();
	BRect rect(std::min(fStart.x, fEnd.x), std::min(fStart.y, fEnd.y),
		std::max(fStart.x, fEnd.x), std::max(fStart.y, fEnd.y));
	BView* view = fCanvas->BeginDrawing();
	if (fShape == SHAPE_LINE) {
		SetUpPen(view, ColorFor(fButtons), size);
		if (fStart == fEnd)
			DrawDot(view, fStart, size);
		else
			view->StrokeLine(fStart, fEnd);
	} else if (fStart != fEnd) {
		// As in the original: the outline is always the primary colour,
		// and the secondary button fills the shape with the secondary one.
		if ((fButtons & B_SECONDARY_MOUSE_BUTTON) != 0) {
			view->SetHighColor(gSettings->SecondaryColor());
			if (fShape == SHAPE_RECTANGLE)
				view->FillRect(rect);
			else
				view->FillEllipse(rect);
		}
		SetUpPen(view, gSettings->PrimaryColor(), size);
		if (fShape == SHAPE_RECTANGLE)
			view->StrokeRect(rect);
		else
			view->StrokeEllipse(rect);
	}
	fCanvas->EndDrawing();

	BRect drawn = StrokeBounds(fStart, fEnd, size);
	fCanvas->InvalidateImage(fDrawn.IsValid() ? drawn | fDrawn : drawn);
	fDrawn = drawn;
}


// #pragma mark - CurveTool


CurveTool::CurveTool(CanvasView* canvas)
	:
	Tool(canvas),
	fStep(0),
	fDrawing(false),
	fButtons(0)
{
}


void
CurveTool::MouseDown(BPoint point, uint32 buttons)
{
	if (!IsDrawingButton(buttons))
		return;
	switch (fStep) {
		case 0:
			// One undo step for the whole curve.
			fCanvas->PushUndo();
			fCanvas->SavePreviewBase();
			fPoints[0] = fPoints[1] = fPoints[2] = fPoints[3] = point;
			fStep = 1;
			break;
		case 1:
			fPoints[1] = fPoints[2] = point;
			fStep = 2;
			break;
		case 2:
			fPoints[2] = point;
			fStep = 3;
			break;
	}
	fDrawing = true;
	fButtons = buttons;
	_Redraw();
}


void
CurveTool::MouseMoved(BPoint point, uint32 buttons)
{
	if (!fDrawing)
		return;
	switch (fStep) {
		case 1:
			fPoints[3] = point;
			break;
		case 2:
			fPoints[1] = fPoints[2] = point;
			break;
		case 3:
			fPoints[2] = point;
			break;
	}
	_Redraw();
}


void
CurveTool::MouseUp(BPoint point, uint32 buttons)
{
	if (!fDrawing)
		return;
	MouseMoved(point, buttons);
	fDrawing = false;
	if (fStep == 3)
		Finish();
}


void
CurveTool::Finish()
{
	if (fStep == 0)
		return;
	fStep = 0;
	fDrawing = false;
	fCanvas->DropPreviewBase();
}


void
CurveTool::_Redraw()
{
	fCanvas->RestorePreviewBase();
	BView* view = fCanvas->BeginDrawing();
	float size = gSettings->PenSize();
	SetUpPen(view, ColorFor(fButtons), size);
	if (fPoints[0] == fPoints[3] && fPoints[1] == fPoints[0])
		DrawDot(view, fPoints[0], size);
	else
		view->StrokeBezier(fPoints);
	fCanvas->EndDrawing();
	fCanvas->InvalidateImage(BRect());
}


// #pragma mark - FillTool


FillTool::FillTool(CanvasView* canvas)
	:
	Tool(canvas)
{
}


void
FillTool::MouseDown(BPoint point, uint32 buttons)
{
	if (!IsDrawingButton(buttons))
		return;
	fCanvas->PushUndo();
	int64 filled = ImageOps::FloodFill(fCanvas->Pixels(), (int32)point.x,
		(int32)point.y, PixelOf(ColorFor(buttons)));
	if (filled == 0) {
		fCanvas->DropLastUndo();
		return;
	}
	fCanvas->SetModified(true);
	fCanvas->InvalidateImage(BRect());
}


const BCursor*
FillTool::Cursor(BPoint viewPoint)
{
	return Cursors::FromResource("cursor_fill");
}


// #pragma mark - ColorPickerTool


ColorPickerTool::ColorPickerTool(CanvasView* canvas)
	:
	Tool(canvas),
	fButtons(0)
{
}


void
ColorPickerTool::MouseDown(BPoint point, uint32 buttons)
{
	fButtons = buttons;
}


void
ColorPickerTool::MouseUp(BPoint point, uint32 buttons)
{
	rgb_color color;
	if (!IsDrawingButton(fButtons) || !fCanvas->PickColor(point, color))
		return;

	BMessage setColor(MSG_SET_COLOR);
	setColor.AddInt32("which",
		(fButtons & B_SECONDARY_MOUSE_BUTTON) != 0 ? 1 : 0);
	color.alpha = 255;
	setColor.AddColor("color", color);
	be_app->PostMessage(&setColor);

	// Hand back to the tool that was in use before the picker.
	BMessage setTool(MSG_SET_TOOL);
	setTool.AddInt32("tool", gSettings->PreviousTool());
	be_app->PostMessage(&setTool);
}


const BCursor*
ColorPickerTool::Cursor(BPoint viewPoint)
{
	return Cursors::FromResource("cursor_pipette");
}


// #pragma mark - MagnifierTool


MagnifierTool::MagnifierTool(CanvasView* canvas)
	:
	Tool(canvas)
{
}


void
MagnifierTool::MouseDown(BPoint point, uint32 buttons)
{
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0)
		fCanvas->ZoomOut(fCanvas->LastPointer());
	else if ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0)
		fCanvas->ZoomIn(fCanvas->LastPointer());
}


const BCursor*
MagnifierTool::Cursor(BPoint viewPoint)
{
	return Cursors::FromResource("cursor_loupe");
}


// #pragma mark - MarqueeTool


MarqueeTool::MarqueeTool(CanvasView* canvas)
	:
	Tool(canvas),
	fHasRect(false),
	fX(0),
	fY(0),
	fWidth(0),
	fHeight(0)
{
}


void
MarqueeTool::DrawOverlay(BView* view)
{
	if (!fHasRect)
		return;
	BRect frame = fCanvas->ToView(_Rect());
	view->SetDrawingMode(B_OP_COPY);
	view->SetPenSize(1);
	view->SetHighColor(0, 0, 255);
	view->SetLowColor(255, 255, 255);
	view->StrokeRect(frame, kStripes);
	view->FillRect(BRect(frame.right + 1, frame.bottom + 1,
		frame.right + kGripSize, frame.bottom + kGripSize));
}


bool
MarqueeTool::_InRect(BPoint point) const
{
	return fHasRect && _Rect().Contains(point);
}


bool
MarqueeTool::_InGrip(BPoint viewPoint) const
{
	if (!fHasRect)
		return false;
	BRect frame = fCanvas->ToView(_Rect());
	return viewPoint.x > frame.right && viewPoint.y > frame.bottom
		&& viewPoint.x <= frame.right + kGripSize + 1
		&& viewPoint.y <= frame.bottom + kGripSize + 1;
}


BRect
MarqueeTool::_Rect() const
{
	return BRect(fX, fY, fX + fWidth - 1, fY + fHeight - 1);
}


static BPoint
ClampToImage(CanvasView* canvas, BPoint point)
{
	point.x = std::max(0.0f, std::min(point.x, canvas->ImageWidth() - 1.0f));
	point.y = std::max(0.0f, std::min(point.y, canvas->ImageHeight() - 1.0f));
	return point;
}


void
MarqueeTool::_StartDrag(BPoint point)
{
	fAnchor = ClampToImage(fCanvas, point);
	fX = (int32)fAnchor.x;
	fY = (int32)fAnchor.y;
	fWidth = fHeight = 1;
	fHasRect = true;
}


void
MarqueeTool::_DragTo(BPoint point)
{
	point = ClampToImage(fCanvas, point);
	fX = (int32)std::min(point.x, fAnchor.x);
	fY = (int32)std::min(point.y, fAnchor.y);
	fWidth = (int32)fabsf(point.x - fAnchor.x) + 1;
	fHeight = (int32)fabsf(point.y - fAnchor.y) + 1;
}


void
MarqueeTool::_Invalidate()
{
	fCanvas->Invalidate();
}


// #pragma mark - SelectionTool


SelectionTool::SelectionTool(CanvasView* canvas)
	:
	MarqueeTool(canvas),
	fState(STATE_IDLE),
	fFloating(NULL)
{
}


SelectionTool::~SelectionTool()
{
	delete fFloating;
}


void
SelectionTool::MouseDown(BPoint point, uint32 buttons)
{
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
		_ShowMenu();
		return;
	}
	if ((buttons & B_PRIMARY_MOUSE_BUTTON) == 0)
		return;

	if (fHasRect) {
		if (_InGrip(fCanvas->LastPointer())) {
			if (fFloating == NULL)
				_Lift();
			fState = STATE_RESIZING;
			return;
		}
		if (_InRect(point)) {
			if (fFloating == NULL)
				_Lift();
			fGrabOffset = BPoint(point.x - fX, point.y - fY);
			fState = STATE_MOVING;
			return;
		}
		Finish();
	}

	_StartDrag(point);
	fState = STATE_SELECTING;
	_Invalidate();
}


void
SelectionTool::MouseMoved(BPoint point, uint32 buttons)
{
	switch (fState) {
		case STATE_SELECTING:
			_DragTo(point);
			break;
		case STATE_MOVING:
			fX = (int32)(point.x - fGrabOffset.x);
			fY = (int32)(point.y - fGrabOffset.y);
			break;
		case STATE_RESIZING:
			fWidth = std::max((int32)1, (int32)point.x - fX + 1);
			fHeight = std::max((int32)1, (int32)point.y - fY + 1);
			break;
		default:
			return;
	}
	_Invalidate();
}


void
SelectionTool::MouseUp(BPoint point, uint32 buttons)
{
	if (fState == STATE_SELECTING) {
		_DragTo(point);
		// A click without a drag selects nothing.
		fHasRect = fWidth > 1 || fHeight > 1;
		_Invalidate();
		fCanvas->NotifyChanged();
	}
	fState = STATE_IDLE;
}


void
SelectionTool::Finish()
{
	_Commit();
	if (fHasRect) {
		fHasRect = false;
		_Invalidate();
		fCanvas->NotifyChanged();
	}
	fState = STATE_IDLE;
}


void
SelectionTool::DrawOverlay(BView* view)
{
	if (fFloating != NULL) {
		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		view->DrawBitmap(fFloating, fFloating->Bounds(),
			fCanvas->ToView(_Rect()));
	}
	MarqueeTool::DrawOverlay(view);
}


const BCursor*
SelectionTool::Cursor(BPoint viewPoint)
{
	if (fState == STATE_RESIZING || _InGrip(viewPoint))
		return Cursors::System(B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST);
	if (fState == STATE_MOVING || _InRect(fCanvas->ToImage(viewPoint)))
		return Cursors::System(B_CURSOR_ID_MOVE);
	return Cursors::System(B_CURSOR_ID_CROSS_HAIR);
}


void
SelectionTool::Copy()
{
	if (!fHasRect)
		return;
	BBitmap* bitmap = _SelectedBitmap();
	ImageIO::CopyToClipboard(bitmap);
	delete bitmap;
}


void
SelectionTool::Delete()
{
	if (!fHasRect)
		return;
	if (fFloating != NULL) {
		// The hole was cleared when the selection was lifted.
		delete fFloating;
		fFloating = NULL;
	} else {
		fCanvas->PushUndo();
		ImageOps::FillRect(fCanvas->Pixels(), fX, fY, fWidth, fHeight,
			PixelOf(kBackgroundWhite));
	}
	fHasRect = false;
	fState = STATE_IDLE;
	fCanvas->SetModified(true);
	_Invalidate();
}


void
SelectionTool::Paste(BBitmap* bitmap, BPoint where)
{
	Finish();
	fCanvas->PushUndo();
	fFloating = bitmap;
	fX = (int32)where.x;
	fY = (int32)where.y;
	fWidth = bitmap->Bounds().IntegerWidth() + 1;
	fHeight = bitmap->Bounds().IntegerHeight() + 1;
	fHasRect = true;
	fState = STATE_IDLE;
	fCanvas->SetModified(true);
	_Invalidate();
}


void
SelectionTool::SelectAll()
{
	Finish();
	fX = fY = 0;
	fWidth = fCanvas->ImageWidth();
	fHeight = fCanvas->ImageHeight();
	fHasRect = true;
	_Invalidate();
	fCanvas->NotifyChanged();
}


// Takes the selected pixels off the image so they can move, leaving a
// white hole, as the original did.
void
SelectionTool::_Lift()
{
	fCanvas->PushUndo();
	fFloating = ImageIO::CopyRect(fCanvas->Image(), _Rect());
	ImageOps::FillRect(fCanvas->Pixels(), fX, fY, fWidth, fHeight,
		PixelOf(kBackgroundWhite));
	fCanvas->SetModified(true);
}


void
SelectionTool::_Commit()
{
	if (fFloating == NULL)
		return;
	BView* view = fCanvas->BeginDrawing();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
	view->DrawBitmap(fFloating, fFloating->Bounds(), _Rect());
	fCanvas->EndDrawing();
	delete fFloating;
	fFloating = NULL;
	fCanvas->SetModified(true);
	_Invalidate();
}


BBitmap*
SelectionTool::_SelectedBitmap() const
{
	if (fFloating == NULL)
		return ImageIO::CopyRect(fCanvas->Image(), _Rect());

	BRect bounds = fFloating->Bounds();
	if (bounds.IntegerWidth() + 1 == fWidth
		&& bounds.IntegerHeight() + 1 == fHeight) {
		return ImageIO::Copy(fFloating);
	}
	BBitmap* scaled = ImageIO::NewImage(fWidth, fHeight);
	if (scaled != NULL)
		ImageOps::Scale(PixelsOf(fFloating), PixelsOf(scaled));
	return scaled;
}


void
SelectionTool::_ShowMenu()
{
	BPopUpMenu* menu = new BPopUpMenu("selection", false, false);
	BMenuItem* item;
	menu->AddItem(item = new BMenuItem(B_TRANSLATE("Cut"),
		new BMessage(B_CUT)));
	item->SetEnabled(fHasRect);
	menu->AddItem(item = new BMenuItem(B_TRANSLATE("Copy"),
		new BMessage(B_COPY)));
	item->SetEnabled(fHasRect);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Paste"), new BMessage(B_PASTE)));
	menu->AddItem(item = new BMenuItem(B_TRANSLATE("Delete"),
		new BMessage(MSG_DELETE_SELECTION)));
	item->SetEnabled(fHasRect);
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem(B_TRANSLATE("Select all"),
		new BMessage(B_SELECT_ALL)));
	menu->SetTargetForItems(fCanvas->Window());
	menu->SetAsyncAutoDestruct(true);
	menu->Go(fCanvas->ConvertToScreen(fCanvas->LastPointer()), true, false,
		true);
}


// #pragma mark - TextTool


TextTool::TextTool(CanvasView* canvas)
	:
	MarqueeTool(canvas),
	fSelecting(false),
	fMoving(false),
	fUndoPushed(false)
{
}


TextTool::~TextTool()
{
	_CloseWindow();
}


void
TextTool::MouseDown(BPoint point, uint32 buttons)
{
	if ((buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
		if (_InRect(point))
			_OpenWindow();
		return;
	}
	if ((buttons & B_PRIMARY_MOUSE_BUTTON) == 0)
		return;

	if (fHasRect && _InRect(point)) {
		fMoving = true;
		fGrabOffset = BPoint(point.x - fX, point.y - fY);
		return;
	}
	if (fHasRect)
		Done(false);

	fSelecting = true;
	_StartDrag(point);
	_Invalidate();
}


void
TextTool::MouseMoved(BPoint point, uint32 buttons)
{
	if (fSelecting) {
		_DragTo(point);
		_Invalidate();
	} else if (fMoving) {
		fX = (int32)(point.x - fGrabOffset.x);
		fY = (int32)(point.y - fGrabOffset.y);
		_Render();
	}
}


void
TextTool::MouseUp(BPoint point, uint32 buttons)
{
	fMoving = false;
	if (!fSelecting)
		return;
	fSelecting = false;
	_DragTo(point);
	if (fWidth <= 1 && fHeight <= 1) {
		fHasRect = false;
		_Invalidate();
		return;
	}
	fCanvas->SavePreviewBase();
	fUndoPushed = false;
	fText = "";
	_OpenWindow();
	_Invalidate();
}


void
TextTool::Finish()
{
	fSelecting = false;
	fMoving = false;
	if (fHasRect)
		Done(false);
}


const BCursor*
TextTool::Cursor(BPoint viewPoint)
{
	if (fMoving || _InRect(fCanvas->ToImage(viewPoint)))
		return Cursors::System(B_CURSOR_ID_MOVE);
	return Cursors::System(B_CURSOR_ID_CROSS_HAIR);
}


void
TextTool::SetText(const char* text)
{
	if (!fHasRect || fSelecting)
		return;
	fText = text;
	if (!fUndoPushed) {
		fCanvas->PushUndo();
		fUndoPushed = true;
	}
	_Render();
}


void
TextTool::Done(bool cancel)
{
	if (!fHasRect)
		return;
	if (cancel || fText.IsEmpty()) {
		fCanvas->RestorePreviewBase();
		if (fUndoPushed)
			fCanvas->DropLastUndo();
	}
	_CloseWindow();
	fCanvas->DropPreviewBase();
	fHasRect = false;
	fUndoPushed = false;
	fText = "";
	_Invalidate();
	fCanvas->NotifyChanged();
}


void
TextTool::_Render()
{
	fCanvas->RestorePreviewBase();
	if (!fText.IsEmpty()) {
		BView* view = fCanvas->BeginDrawing();
		BFont font = gSettings->TextFont();
		view->SetFont(&font);
		view->SetHighColor(gSettings->PrimaryColor());
		view->SetDrawingMode(B_OP_OVER);
		BRegion clip(_Rect());
		view->ConstrainClippingRegion(&clip);

		font_height height;
		font.GetHeight(&height);
		float lineHeight = ceilf(height.ascent + height.descent
			+ height.leading);
		float y = fY + ceilf(height.ascent);
		int32 start = 0;
		while (start <= fText.Length()) {
			int32 end = fText.FindFirst('\n', start);
			if (end < 0)
				end = fText.Length();
			BString line;
			fText.CopyInto(line, start, end - start);
			view->DrawString(line.String(), BPoint(fX, y));
			y += lineHeight;
			start = end + 1;
		}
		view->ConstrainClippingRegion(NULL);
		fCanvas->EndDrawing();
		fCanvas->SetModified(true);
	}
	_Invalidate();
}


void
TextTool::_OpenWindow()
{
	if (fWindow.IsValid())
		return;
	TextWindow* window = new TextWindow(BMessenger(fCanvas), fText.String(),
		fCanvas->Window());
	fWindow = BMessenger(window);
	window->Show();
}


void
TextTool::_CloseWindow()
{
	if (fWindow.IsValid())
		fWindow.SendMessage(MSG_TEXT_CLOSE);
	fWindow = BMessenger();
}
