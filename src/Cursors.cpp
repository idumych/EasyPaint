/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "Cursors.h"

#include <Autolock.h>
#include <Bitmap.h>
#include <Locker.h>
#include <String.h>
#include <TranslationUtils.h>

#include <math.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>


namespace Cursors {

static BLocker sLock("cursors");
static std::map<int32, BCursor*> sSystem;
static std::map<std::string, BCursor*> sResources;
static BCursor* sBrush = NULL;
static float sBrushDiameter = -1;
static rgb_color sBrushFill;


const BCursor*
System(BCursorID id)
{
	BAutolock _(sLock);
	BCursor*& cursor = sSystem[id];
	if (cursor == NULL)
		cursor = new BCursor(id);
	return cursor;
}


const BCursor*
FromResource(const char* name)
{
	BAutolock _(sLock);
	BCursor*& cursor = sResources[name];
	if (cursor == NULL) {
		BBitmap* bitmap = BTranslationUtils::GetBitmap('PNG ', name);
		if (bitmap == NULL)
			return System(B_CURSOR_ID_CROSS_HAIR);
		// The original used Qt's default hot spot, the middle.
		BRect bounds = bitmap->Bounds();
		cursor = new BCursor(bitmap,
			BPoint(floorf(bounds.Width() / 2), floorf(bounds.Height() / 2)));
		delete bitmap;
	}
	return cursor;
}


static inline void
SetPixel(BBitmap* bitmap, int32 x, int32 y, rgb_color color)
{
	BRect bounds = bitmap->Bounds();
	if (x < 0 || y < 0 || x > bounds.right || y > bounds.bottom)
		return;
	uint8* pixel = (uint8*)bitmap->Bits() + y * bitmap->BytesPerRow() + x * 4;
	pixel[0] = color.blue;
	pixel[1] = color.green;
	pixel[2] = color.red;
	pixel[3] = color.alpha;
}


// A filled circle the size of the brush, with the original's dotted
// cross hair through the middle.
const BCursor*
Brush(float diameter, rgb_color fill)
{
	BAutolock _(sLock);
	diameter = std::min(std::max(diameter, 1.0f), 63.0f);
	if (sBrush != NULL && diameter == sBrushDiameter
		&& fill == sBrushFill) {
		return sBrush;
	}

	int32 size = std::max((int32)ceilf(diameter) + 2, (int32)25);
	if (size % 2 == 0)
		size++;
	int32 center = size / 2;
	BBitmap bitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	memset(bitmap.Bits(), 0, bitmap.BitsLength());

	rgb_color outline = make_color(0, 0, 0);
	fill.alpha = 255;
	float radius = diameter / 2;
	for (int32 y = 0; y < size; y++) {
		for (int32 x = 0; x < size; x++) {
			float distance = hypotf(x - center, y - center);
			if (distance <= radius - 0.5f)
				SetPixel(&bitmap, x, y, fill);
			else if (distance <= radius + 0.5f)
				SetPixel(&bitmap, x, y, outline);
		}
	}

	rgb_color black = make_color(0, 0, 0);
	rgb_color white = make_color(255, 255, 255);
	static const int32 kDots[] = { 10, 8, -8, -10 };
	SetPixel(&bitmap, center, center, black);
	for (int32 offset : kDots) {
		SetPixel(&bitmap, center + offset, center, black);
		SetPixel(&bitmap, center, center + offset, black);
		int32 between = offset > 0 ? offset - 1 : offset + 1;
		SetPixel(&bitmap, center + between, center, white);
		SetPixel(&bitmap, center, center + between, white);
	}
	SetPixel(&bitmap, center - 1, center, white);
	SetPixel(&bitmap, center + 1, center, white);
	SetPixel(&bitmap, center, center - 1, white);
	SetPixel(&bitmap, center, center + 1, white);

	// The previous brush may still be the view's cursor; the app_server
	// keeps its own reference, so it can go.
	delete sBrush;
	sBrush = new BCursor(&bitmap, BPoint(center, center));
	sBrushDiameter = diameter;
	sBrushFill = fill;
	return sBrush;
}

}	// namespace Cursors
