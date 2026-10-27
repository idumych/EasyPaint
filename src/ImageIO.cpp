/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "ImageIO.h"

#include <BitmapStream.h>
#include <Clipboard.h>
#include <DataIO.h>
#include <Directory.h>
#include <File.h>
#include <Message.h>
#include <Mime.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Path.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <string.h>
#include <strings.h>

#include "ImageOps.h"


namespace ImageIO {

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


BBitmap*
NewImage(int32 width, int32 height, rgb_color fill, uint32 flags)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), flags,
		B_RGBA32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	ImageOps::Pixels pixels = PixelsOf(bitmap);
	ImageOps::FillRect(pixels, 0, 0, width, height,
		ImageOps::MakePixel(fill.red, fill.green, fill.blue, fill.alpha));
	return bitmap;
}


BBitmap*
ConvertToRGBA32(const BBitmap* source)
{
	if (source == NULL)
		return NULL;
	BRect bounds = source->Bounds();
	BBitmap* bitmap = NewImage(bounds.IntegerWidth() + 1,
		bounds.IntegerHeight() + 1);
	if (bitmap == NULL)
		return NULL;

	color_space space = source->ColorSpace();
	if (space == B_RGBA32) {
		ImageOps::Pixels from = PixelsOf(source);
		ImageOps::Blit(from, 0, 0, from.width, from.height, PixelsOf(bitmap),
			0, 0);
		return bitmap;
	}

	if (bitmap->ImportBits(source) != B_OK) {
		delete bitmap;
		return NULL;
	}
	// These carry no alpha, and the alpha byte of B_RGB32 is undefined.
	if (space == B_RGB32 || space == B_RGB24 || space == B_RGB16
		|| space == B_RGB15 || space == B_GRAY8 || space == B_GRAY1) {
		ImageOps::Pixels pixels = PixelsOf(bitmap);
		for (int32 y = 0; y < pixels.height; y++) {
			for (int32 x = 0; x < pixels.width; x++)
				pixels.At(x, y) |= 0xff000000;
		}
	}
	return bitmap;
}


BBitmap*
Copy(const BBitmap* source)
{
	return CopyRect(source, source->Bounds());
}


BBitmap*
CopyRect(const BBitmap* source, BRect rect)
{
	int32 width = rect.IntegerWidth() + 1;
	int32 height = rect.IntegerHeight() + 1;
	BBitmap* bitmap = NewImage(width, height, make_color(0, 0, 0, 0));
	if (bitmap == NULL)
		return NULL;
	ImageOps::Blit(PixelsOf(source), (int32)rect.left, (int32)rect.top,
		width, height, PixelsOf(bitmap), 0, 0);
	return bitmap;
}


BBitmap*
Load(const entry_ref& ref)
{
	BBitmap* translated = BTranslationUtils::GetBitmap(&ref);
	BBitmap* bitmap = ConvertToRGBA32(translated);
	delete translated;
	return bitmap;
}


bool
IsImage(const entry_ref& ref)
{
	BFile file(&ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	translator_info info;
	return BTranslatorRoster::Default()->Identify(&file, NULL, &info, 0,
			NULL, B_TRANSLATOR_BITMAP) == B_OK;
}


static const struct {
	const char*	extension;
	const char*	mimeType;
} kFormats[] = {
	{ "png", "image/png" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "bmp", "image/bmp" },
	{ "gif", "image/gif" },
	{ "tif", "image/tiff" },
	{ "tiff", "image/tiff" },
	{ "tga", "image/x-targa" },
	{ "webp", "image/webp" },
	{ "ppm", "image/x-portable-pixmap" },
	{ "avif", "image/avif" },
	{ "jp2", "image/jp2" },
	{ "ico", "image/vnd.microsoft.icon" },
	{ "psd", "image/vnd.adobe.photoshop" },
};


// Finds a translator that writes bitmaps as mimeType.
static bool
FindWriter(const char* mimeType, translator_id& translator, uint32& type)
{
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	translator_id* ids = NULL;
	int32 count = 0;
	if (roster->GetAllTranslators(&ids, &count) != B_OK)
		return false;

	bool found = false;
	for (int32 i = 0; i < count && !found; i++) {
		const translation_format* formats = NULL;
		int32 formatCount = 0;
		if (roster->GetOutputFormats(ids[i], &formats, &formatCount) != B_OK)
			continue;
		for (int32 j = 0; j < formatCount; j++) {
			if (formats[j].group == B_TRANSLATOR_BITMAP
				&& formats[j].type != B_TRANSLATOR_BITMAP
				&& strcasecmp(formats[j].MIME, mimeType) == 0) {
				translator = ids[i];
				type = formats[j].type;
				found = true;
				break;
			}
		}
	}
	delete[] ids;
	return found;
}


static const char*
MimeTypeFor(const entry_ref& directory, const char* name)
{
	// An existing image keeps its type, even without an extension.
	BEntry entry;
	BDirectory dir(&directory);
	if (entry.SetTo(&dir, name) == B_OK && entry.Exists()) {
		BNode node(&entry);
		BNodeInfo info(&node);
		static char type[B_MIME_TYPE_LENGTH];
		translator_id translator;
		uint32 format;
		if (info.GetType(type) == B_OK && strncmp(type, "image/", 6) == 0
			&& FindWriter(type, translator, format)) {
			return type;
		}
	}

	const char* dot = strrchr(name, '.');
	if (dot != NULL) {
		for (const auto& format : kFormats) {
			if (strcasecmp(dot + 1, format.extension) == 0)
				return format.mimeType;
		}
	}
	return "image/png";
}


status_t
Save(const BBitmap* bitmap, const entry_ref& directory, const char* name,
	BString* error)
{
	const char* mimeType = MimeTypeFor(directory, name);
	translator_id translator;
	uint32 type;
	if (!FindWriter(mimeType, translator, type)) {
		// Like the original: an unknown format is saved as PNG.
		mimeType = "image/png";
		if (!FindWriter(mimeType, translator, type)) {
			if (error != NULL)
				error->SetTo("No translator can write PNG images.");
			return B_NOT_SUPPORTED;
		}
	}

	BDirectory dir(&directory);
	BFile file;
	status_t status = dir.CreateFile(name, &file, false);
	if (status != B_OK) {
		if (error != NULL)
			error->SetTo(strerror(status));
		return status;
	}

	BBitmap* copy = Copy(bitmap);
	if (copy == NULL)
		return B_NO_MEMORY;
	// The stream deletes the copy.
	BBitmapStream stream(copy);
	status = BTranslatorRoster::Default()->Translate(translator, &stream,
		NULL, &file, type);
	if (status != B_OK) {
		if (error != NULL)
			error->SetTo(strerror(status));
		return status;
	}
	file.SetSize(file.Position());

	BNodeInfo info(&file);
	info.SetType(mimeType);
	return B_OK;
}


status_t
CopyToClipboard(const BBitmap* bitmap)
{
	if (bitmap == NULL || !be_clipboard->Lock())
		return B_ERROR;

	be_clipboard->Clear();
	BMessage* clip = be_clipboard->Data();
	BMessage archive;
	status_t status = bitmap->Archive(&archive);
	if (status == B_OK) {
		// The same layout as ShowImage's "Copy".
		clip->AddMessage("image/bitmap", &archive);
		clip->AddRect("rect", bitmap->Bounds());
		status = be_clipboard->Commit();
	}
	be_clipboard->Unlock();
	return status;
}


BBitmap*
PasteFromClipboard()
{
	if (!be_clipboard->Lock())
		return NULL;

	BBitmap* bitmap = NULL;
	BMessage* clip = be_clipboard->Data();
	BMessage archive;
	if (clip != NULL && clip->FindMessage("image/bitmap", &archive) == B_OK) {
		BBitmap* archived = new BBitmap(&archive);
		if (archived->InitCheck() == B_OK)
			bitmap = ConvertToRGBA32(archived);
		delete archived;
	}

	// Raw image data under its MIME type, as some apps put it there.
	if (bitmap == NULL && clip != NULL) {
		char* name;
		type_code type;
		for (int32 i = 0; clip->GetInfo(B_MIME_TYPE, i, &name, &type) == B_OK;
				i++) {
			if (strncmp(name, "image/", 6) != 0)
				continue;
			const void* data;
			ssize_t size;
			if (clip->FindData(name, B_MIME_TYPE, &data, &size) != B_OK)
				continue;
			BMemoryIO io(data, size);
			BBitmap* translated = BTranslationUtils::GetBitmap(&io);
			bitmap = ConvertToRGBA32(translated);
			delete translated;
			if (bitmap != NULL)
				break;
		}
	}

	be_clipboard->Unlock();
	return bitmap;
}


bool
ClipboardImageSize(int32& width, int32& height)
{
	BBitmap* bitmap = PasteFromClipboard();
	if (bitmap == NULL)
		return false;
	width = bitmap->Bounds().IntegerWidth() + 1;
	height = bitmap->Bounds().IntegerHeight() + 1;
	delete bitmap;
	return true;
}

}	// namespace ImageIO
