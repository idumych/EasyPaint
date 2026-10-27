/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "CanvasView.h"

#include <Bitmap.h>
#include <Message.h>
#include <Region.h>
#include <ScrollBar.h>
#include <Window.h>

#include <math.h>
#include <string.h>

#include <algorithm>

#include "Cursors.h"
#include "ImageIO.h"
#include "Settings.h"
#include "Tools.h"


static const float kGripSize = 6;
static const float kMinZoom = 0.25f;
static const float kMaxZoom = 16.0f;
static const rgb_color kDeskColor = { 128, 128, 128, 255 };


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


// Returns a bitmap the canvas can draw into, taking ownership of image.
static BBitmap*
DrawableImage(BBitmap* image)
{
	if ((image->Flags() & B_BITMAP_ACCEPTS_VIEWS) != 0
		&& image->ColorSpace() == B_RGBA32) {
		return image;
	}
	BRect bounds = image->Bounds();
	BBitmap* drawable = ImageIO::NewImage(bounds.IntegerWidth() + 1,
		bounds.IntegerHeight() + 1, kBackgroundWhite, B_BITMAP_ACCEPTS_VIEWS);
	BBitmap* converted = ImageIO::ConvertToRGBA32(image);
	if (drawable != NULL && converted != NULL) {
		ImageOps::Pixels from = PixelsOf(converted);
		ImageOps::Blit(from, 0, 0, from.width, from.height,
			PixelsOf(drawable), 0, 0);
	}
	delete converted;
	delete image;
	return drawable;
}


CanvasView::CanvasView(BBitmap* image)
	:
	BView("canvas", B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
	fImage(NULL),
	fImageView(NULL),
	fPreviewBase(NULL),
	fActiveTool(NULL),
	fZoom(1.0f),
	fModified(false),
	fButtons(0),
	fResizingCanvas(false),
	fResizeOriginal(NULL),
	fLastPointer(-1, -1)
{
	SetViewColor(B_TRANSPARENT_COLOR);

	fImage = DrawableImage(image);
	fImageView = new BView(fImage->Bounds(), "image", B_FOLLOW_NONE, 0);
	fImage->AddChild(fImageView);

	fSelectionTool = new SelectionTool(this);
	fTextTool = new TextTool(this);
	fTools[TOOL_SELECTION] = fSelectionTool;
	fTools[TOOL_ERASER] = new PenTool(this, true);
	fTools[TOOL_COLOR_PICKER] = new ColorPickerTool(this);
	fTools[TOOL_MAGNIFIER] = new MagnifierTool(this);
	fTools[TOOL_PEN] = new PenTool(this, false);
	fTools[TOOL_LINE] = new ShapeTool(this, ShapeTool::SHAPE_LINE);
	fTools[TOOL_SPRAY] = new SprayTool(this);
	fTools[TOOL_FILL] = new FillTool(this);
	fTools[TOOL_RECTANGLE] = new ShapeTool(this, ShapeTool::SHAPE_RECTANGLE);
	fTools[TOOL_ELLIPSE] = new ShapeTool(this, ShapeTool::SHAPE_ELLIPSE);
	fTools[TOOL_CURVE] = new CurveTool(this);
	fTools[TOOL_TEXT] = fTextTool;
}


CanvasView::~CanvasView()
{
	for (int32 i = 0; i < TOOL_COUNT; i++)
		delete fTools[i];
	_ClearHistory(fUndo);
	_ClearHistory(fRedo);
	delete fPreviewBase;
	delete fResizeOriginal;
	delete fImage;
}


// #pragma mark - The image


int32
CanvasView::ImageWidth() const
{
	return fImage->Bounds().IntegerWidth() + 1;
}


int32
CanvasView::ImageHeight() const
{
	return fImage->Bounds().IntegerHeight() + 1;
}


BRect
CanvasView::ImageBounds() const
{
	return fImage->Bounds();
}


ImageOps::Pixels
CanvasView::Pixels() const
{
	return PixelsOf(fImage);
}


BBitmap*
CanvasView::CopyImage() const
{
	return ImageIO::Copy(fImage);
}


void
CanvasView::ReplaceImage(BBitmap* image, bool pushUndo)
{
	if (image == NULL)
		return;
	if (pushUndo)
		PushUndo();

	fImage->RemoveChild(fImageView);
	delete fImage;
	fImage = DrawableImage(image);
	fImageView->ResizeTo(fImage->Bounds().Width(), fImage->Bounds().Height());
	fImage->AddChild(fImageView);

	delete fPreviewBase;
	fPreviewBase = NULL;
	SetModified(true);
	_ImageSizeChanged();
}


bool
CanvasView::IsBusy() const
{
	return fActiveTool != NULL || fResizingCanvas || HasSelection()
		|| fTextTool->IsEditing();
}


void
CanvasView::SetModified(bool modified)
{
	fModified = modified;
	NotifyChanged();
}


// #pragma mark - Drawing for the tools


BView*
CanvasView::BeginDrawing()
{
	fImage->Lock();
	fImageView->SetDrawingMode(B_OP_COPY);
	fImageView->SetPenSize(1);
	fImageView->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	fImageView->ConstrainClippingRegion(NULL);
	return fImageView;
}


void
CanvasView::EndDrawing()
{
	fImageView->Sync();
	fImage->Unlock();
	fModified = true;
}


void
CanvasView::SavePreviewBase()
{
	delete fPreviewBase;
	fPreviewBase = ImageIO::Copy(fImage);
}


void
CanvasView::RestorePreviewBase()
{
	if (fPreviewBase == NULL)
		return;
	ImageOps::Pixels from = PixelsOf(fPreviewBase);
	ImageOps::Blit(from, 0, 0, from.width, from.height, PixelsOf(fImage),
		0, 0);
}


void
CanvasView::DropPreviewBase()
{
	delete fPreviewBase;
	fPreviewBase = NULL;
}


// #pragma mark - Undo


void
CanvasView::PushUndo()
{
	BBitmap* snapshot = ImageIO::Copy(fImage);
	if (snapshot == NULL)
		return;
	fUndo.push_back(snapshot);
	_TrimHistory(fUndo);
	_ClearHistory(fRedo);
	NotifyChanged();
}


void
CanvasView::DropLastUndo()
{
	if (fUndo.empty())
		return;
	delete fUndo.back();
	fUndo.pop_back();
	NotifyChanged();
}


void
CanvasView::Undo()
{
	FinishTools();
	if (fUndo.empty())
		return;
	BBitmap* previous = fUndo.back();
	fUndo.pop_back();
	fRedo.push_back(ImageIO::Copy(fImage));
	ReplaceImage(previous, false);
}


void
CanvasView::Redo()
{
	FinishTools();
	if (fRedo.empty())
		return;
	BBitmap* next = fRedo.back();
	fRedo.pop_back();
	fUndo.push_back(ImageIO::Copy(fImage));
	_TrimHistory(fUndo);
	ReplaceImage(next, false);
}


void
CanvasView::_TrimHistory(std::deque<BBitmap*>& history)
{
	size_t depth = std::max(gSettings->HistoryDepth(), (int32)1);
	while (history.size() > depth) {
		delete history.front();
		history.pop_front();
	}
}


void
CanvasView::_ClearHistory(std::deque<BBitmap*>& history)
{
	for (BBitmap* bitmap : history)
		delete bitmap;
	history.clear();
}


// #pragma mark - Selection and clipboard


bool
CanvasView::HasSelection() const
{
	return fSelectionTool->HasSelection();
}


void
CanvasView::Cut()
{
	if (!HasSelection())
		return;
	fSelectionTool->Copy();
	fSelectionTool->Delete();
}


void
CanvasView::Copy()
{
	fSelectionTool->Copy();
}


void
CanvasView::Paste()
{
	BBitmap* bitmap = ImageIO::PasteFromClipboard();
	if (bitmap == NULL)
		return;

	// Paste at the top left of what's visible.
	BPoint where = ToImage(Bounds().LeftTop());
	where.x = std::max(0.0f, std::min(where.x, (float)ImageWidth() - 1));
	where.y = std::max(0.0f, std::min(where.y, (float)ImageHeight() - 1));
	for (int32 i = 0; i < TOOL_COUNT; i++) {
		if (i != TOOL_SELECTION)
			fTools[i]->Finish();
	}
	fSelectionTool->Paste(bitmap, where);
}


void
CanvasView::SelectAll()
{
	FinishTools();
	fSelectionTool->SelectAll();
}


void
CanvasView::DeleteSelection()
{
	fSelectionTool->Delete();
}


void
CanvasView::FinishTools()
{
	for (int32 i = 0; i < TOOL_COUNT; i++)
		fTools[i]->Finish();
}


void
CanvasView::ToolChanged()
{
	FinishTools();
	UpdateCursor();
}


void
CanvasView::SetText(const char* text)
{
	fTextTool->SetText(text);
}


void
CanvasView::TextDone(bool cancel)
{
	fTextTool->Done(cancel);
}


// #pragma mark - Image menu and effects


void
CanvasView::ApplySimpleEffect(effect_id effect)
{
	FinishTools();
	PushUndo();
	ImageOps::Pixels pixels = Pixels();
	switch (effect) {
		case EFFECT_GRAY:
			ImageOps::Gray(pixels);
			break;
		case EFFECT_NEGATIVE:
			ImageOps::Negative(pixels);
			break;
		case EFFECT_BINARIZATION:
			ImageOps::Binarize(pixels, 128);
			break;
		case EFFECT_GAMMA:
			ImageOps::Gamma(pixels, 2.0);
			break;
		default:
			break;
	}
	SetModified(true);
	Invalidate();
}


void
CanvasView::ResizeImage(int32 width, int32 height)
{
	FinishTools();
	if (width < 1 || height < 1)
		return;
	BBitmap* scaled = ImageIO::NewImage(width, height);
	if (scaled == NULL)
		return;
	ImageOps::Scale(Pixels(), PixelsOf(scaled));
	ReplaceImage(scaled, true);
}


void
CanvasView::ResizeCanvas(int32 width, int32 height)
{
	FinishTools();
	if (width < 1 || height < 1)
		return;
	BBitmap* canvas = ImageIO::NewImage(width, height);
	if (canvas == NULL)
		return;
	ImageOps::Pixels from = Pixels();
	ImageOps::Blit(from, 0, 0, from.width, from.height, PixelsOf(canvas), 0, 0);
	ReplaceImage(canvas, true);
}


void
CanvasView::Rotate(bool clockwise)
{
	FinishTools();
	BBitmap* rotated = ImageIO::NewImage(ImageHeight(), ImageWidth());
	if (rotated == NULL)
		return;
	ImageOps::Rotate90(Pixels(), PixelsOf(rotated), clockwise);
	ReplaceImage(rotated, true);
}


// #pragma mark - Zoom


void
CanvasView::SetZoom(float zoom, BPoint keepAt)
{
	zoom = std::min(std::max(zoom, kMinZoom), kMaxZoom);
	if (zoom == fZoom)
		return;

	// Keep the image point under keepAt (a view point) where it is on
	// screen, or else the middle of the view.
	BRect bounds = Bounds();
	if (keepAt.x < 0)
		keepAt = BPoint(bounds.left + bounds.Width() / 2,
			bounds.top + bounds.Height() / 2);
	BPoint imagePoint(keepAt.x / fZoom, keepAt.y / fZoom);
	BPoint onScreen = keepAt - bounds.LeftTop();

	fZoom = zoom;
	_UpdateScrollBars();
	BPoint scrollTo(imagePoint.x * fZoom - onScreen.x,
		imagePoint.y * fZoom - onScreen.y);
	scrollTo.x = std::max(0.0f, floorf(scrollTo.x));
	scrollTo.y = std::max(0.0f, floorf(scrollTo.y));
	ScrollTo(scrollTo);
	Invalidate();
	UpdateCursor();
	NotifyChanged();
}


void
CanvasView::ZoomIn(BPoint keepAt)
{
	SetZoom(fZoom * 2, keepAt);
}


void
CanvasView::ZoomOut(BPoint keepAt)
{
	SetZoom(fZoom / 2, keepAt);
}


// #pragma mark - Coordinates


BPoint
CanvasView::ToImage(BPoint viewPoint) const
{
	return BPoint(floorf(viewPoint.x / fZoom), floorf(viewPoint.y / fZoom));
}


BRect
CanvasView::ToView(BRect imageRect) const
{
	return BRect(imageRect.left * fZoom, imageRect.top * fZoom,
		(imageRect.right + 1) * fZoom - 1, (imageRect.bottom + 1) * fZoom - 1);
}


BPoint
CanvasView::ToView(BPoint imagePoint) const
{
	return BPoint(imagePoint.x * fZoom, imagePoint.y * fZoom);
}


bool
CanvasView::PickColor(BPoint imagePoint, rgb_color& color) const
{
	ImageOps::Pixels pixels = Pixels();
	int32 x = (int32)imagePoint.x;
	int32 y = (int32)imagePoint.y;
	if (!pixels.Contains(x, y))
		return false;
	uint32 pixel = pixels.At(x, y);
	color = make_color(ImageOps::Red(pixel), ImageOps::Green(pixel),
		ImageOps::Blue(pixel), ImageOps::Alpha(pixel));
	return true;
}


// #pragma mark - Called by the tools


void
CanvasView::InvalidateImage(BRect imageRect)
{
	if (!imageRect.IsValid())
		imageRect = ImageBounds();
	Invalidate(ToView(imageRect).InsetByCopy(-2, -2));
}


void
CanvasView::NotifyChanged()
{
	if (Window() != NULL)
		Window()->PostMessage(MSG_CANVAS_CHANGED);
}


void
CanvasView::UpdateCursor()
{
	_SetCursorFor(fLastPointer);
}


// #pragma mark - BView


void
CanvasView::AttachedToWindow()
{
	BView::AttachedToWindow();
	_UpdateScrollBars();
	UpdateCursor();
}


void
CanvasView::_DrawCheckerboard(BRect rect)
{
	// Shows through where the image is transparent.
	static const float kSquare = 8;
	SetHighColor(204, 204, 204);
	SetLowColor(255, 255, 255);
	FillRect(rect, B_SOLID_LOW);
	float startX = floorf(rect.left / kSquare) * kSquare;
	float startY = floorf(rect.top / kSquare) * kSquare;
	for (float y = startY; y <= rect.bottom; y += kSquare) {
		for (float x = startX; x <= rect.right; x += kSquare) {
			if (((int32)(x / kSquare) + (int32)(y / kSquare)) % 2 == 0)
				continue;
			BRect square(x, y, x + kSquare - 1, y + kSquare - 1);
			FillRect(square & rect);
		}
	}
}


void
CanvasView::Draw(BRect updateRect)
{
	BRect imageRect = ToView(ImageBounds());

	// The grey desk around the image.
	SetHighColor(kDeskColor);
	BRegion desk(updateRect);
	desk.Exclude(imageRect);
	FillRegion(&desk);

	BRect visible = updateRect & imageRect;
	if (visible.IsValid()) {
		_DrawCheckerboard(visible);

		// Only the part of the image that needs drawing.
		BRect source(floorf(visible.left / fZoom), floorf(visible.top / fZoom),
			ceilf((visible.right + 1) / fZoom) - 1,
			ceilf((visible.bottom + 1) / fZoom) - 1);
		source = source & ImageBounds();
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fImage, source, ToView(source));
		SetDrawingMode(B_OP_COPY);
	}

	// The canvas grip.
	SetHighColor(0, 0, 0);
	FillRect(BRect(imageRect.right + 1, imageRect.bottom + 1,
		imageRect.right + kGripSize, imageRect.bottom + kGripSize));

	Tool* tool = _CurrentTool();
	tool->DrawOverlay(this);
	// The text tool's frame stays visible while its window is open.
	if (tool != fTextTool)
		fTextTool->DrawOverlay(this);
}


void
CanvasView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_UpdateScrollBars();
}


void
CanvasView::KeyDown(const char* bytes, int32 numBytes)
{
	switch (bytes[0]) {
		case B_DELETE:
		case B_BACKSPACE:
			if (HasSelection()) {
				DeleteSelection();
				return;
			}
			break;
		case B_ESCAPE:
			FinishTools();
			return;
	}
	BView::KeyDown(bytes, numBytes);
}


void
CanvasView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_TEXT_CHANGED:
			SetText(message->GetString("text", ""));
			break;
		case MSG_TEXT_DONE:
			TextDone(message->GetBool("cancel", false));
			break;
		default:
			BView::MessageReceived(message);
	}
}


void
CanvasView::MouseDown(BPoint where)
{
	MakeFocus(true);
	fLastPointer = where;
	int32 buttons = 0;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
	// Control-click is the secondary button for one-button mice.
	if (buttons == B_PRIMARY_MOUSE_BUTTON && (modifiers() & B_CONTROL_KEY) != 0)
		buttons = B_SECONDARY_MOUSE_BUTTON;
	fButtons = buttons;
	SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);

	if ((buttons & B_PRIMARY_MOUSE_BUTTON) != 0 && _InCanvasGrip(where)) {
		FinishTools();
		PushUndo();
		fResizingCanvas = true;
		fResizeOriginal = ImageIO::Copy(fImage);
		return;
	}

	fActiveTool = _CurrentTool();
	fActiveTool->MouseDown(ToImage(where), buttons);
}


void
CanvasView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	fLastPointer = transit == B_EXITED_VIEW ? BPoint(-1, -1) : where;

	if (fResizingCanvas) {
		int32 width = std::max((int32)1, (int32)(where.x / fZoom));
		int32 height = std::max((int32)1, (int32)(where.y / fZoom));
		if (width != ImageWidth() || height != ImageHeight()) {
			BBitmap* canvas = ImageIO::NewImage(width, height);
			if (canvas != NULL) {
				ImageOps::Pixels from = PixelsOf(fResizeOriginal);
				ImageOps::Blit(from, 0, 0, from.width, from.height,
					PixelsOf(canvas), 0, 0);
				ReplaceImage(canvas, false);
			}
		}
		return;
	}

	BPoint point = ToImage(where);
	if (fActiveTool != NULL) {
		int32 buttons = 0;
		if (Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("buttons", &buttons);
		if (buttons != 0)
			fActiveTool->MouseMoved(point, fButtons);
	} else
		_CurrentTool()->MouseMoved(point, 0);

	_SetCursorFor(fLastPointer);

	BMessage status(MSG_POINTER_STATUS);
	if (transit != B_EXITED_VIEW && point.x >= 0 && point.y >= 0
		&& point.x < ImageWidth() && point.y < ImageHeight()) {
		status.AddPoint("where", point);
		rgb_color color;
		if (gSettings->Tool() == TOOL_COLOR_PICKER && PickColor(point, color))
			status.AddColor("color", color);
	}
	Window()->PostMessage(&status);
}


void
CanvasView::MouseUp(BPoint where)
{
	if (fResizingCanvas) {
		fResizingCanvas = false;
		delete fResizeOriginal;
		fResizeOriginal = NULL;
		UpdateCursor();
		return;
	}
	if (fActiveTool != NULL) {
		Tool* tool = fActiveTool;
		fActiveTool = NULL;
		tool->MouseUp(ToImage(where), fButtons);
	}
	fButtons = 0;
}


// #pragma mark - Private


Tool*
CanvasView::_CurrentTool() const
{
	tool_id tool = gSettings->Tool();
	if (tool < 0 || tool >= TOOL_COUNT)
		tool = TOOL_PEN;
	return fTools[tool];
}


bool
CanvasView::_InCanvasGrip(BPoint viewPoint) const
{
	BRect imageRect = ToView(ImageBounds());
	return viewPoint.x > imageRect.right && viewPoint.y > imageRect.bottom
		&& viewPoint.x <= imageRect.right + kGripSize + 1
		&& viewPoint.y <= imageRect.bottom + kGripSize + 1;
}


void
CanvasView::_UpdateScrollBars()
{
	BRect bounds = Bounds();
	float dataWidth = ImageWidth() * fZoom + kGripSize + 1;
	float dataHeight = ImageHeight() * fZoom + kGripSize + 1;

	BScrollBar* bar = ScrollBar(B_HORIZONTAL);
	if (bar != NULL) {
		float range = std::max(0.0f, dataWidth - bounds.Width() - 1);
		bar->SetRange(0, range);
		bar->SetProportion(std::min(1.0f, (bounds.Width() + 1) / dataWidth));
		bar->SetSteps(16, std::max(16.0f, bounds.Width() - 16));
	}
	bar = ScrollBar(B_VERTICAL);
	if (bar != NULL) {
		float range = std::max(0.0f, dataHeight - bounds.Height() - 1);
		bar->SetRange(0, range);
		bar->SetProportion(std::min(1.0f, (bounds.Height() + 1) / dataHeight));
		bar->SetSteps(16, std::max(16.0f, bounds.Height() - 16));
	}
}


void
CanvasView::_SetCursorFor(BPoint viewPoint)
{
	if (Window() == NULL)
		return;
	if (fResizingCanvas || (viewPoint.x >= 0 && _InCanvasGrip(viewPoint))) {
		SetViewCursor(Cursors::System(
			B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST));
		return;
	}
	SetViewCursor(_CurrentTool()->Cursor(viewPoint));
}


void
CanvasView::_ImageSizeChanged()
{
	_UpdateScrollBars();
	Invalidate();
	NotifyChanged();
}
