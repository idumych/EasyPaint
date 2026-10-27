/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * Images in and out: files through the Translation Kit, and the system
 * clipboard. Every bitmap handed back is B_RGBA32.
 */
#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include <Bitmap.h>
#include <Entry.h>
#include <GraphicsDefs.h>
#include <String.h>


namespace ImageIO {

BBitmap*	NewImage(int32 width, int32 height,
				rgb_color fill = make_color(255, 255, 255), uint32 flags = 0);
// A B_RGBA32 copy of any bitmap. Colour spaces without alpha come out
// opaque.
BBitmap*	ConvertToRGBA32(const BBitmap* source);
BBitmap*	Copy(const BBitmap* source);
BBitmap*	CopyRect(const BBitmap* source, BRect rect);

BBitmap*	Load(const entry_ref& ref);
// Picks the format from the file's MIME type if it already exists and is
// an image, otherwise from the name's extension; PNG if neither is known.
status_t	Save(const BBitmap* bitmap, const entry_ref& directory,
				const char* name, BString* error);
// True if a translator can read the file.
bool		IsImage(const entry_ref& ref);

status_t	CopyToClipboard(const BBitmap* bitmap);
BBitmap*	PasteFromClipboard();
bool		ClipboardImageSize(int32& width, int32& height);

}	// namespace ImageIO

#endif	// IMAGE_IO_H
