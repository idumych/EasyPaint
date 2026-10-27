/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "EffectWindow.h"

#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <SpaceLayoutItem.h>
#include <Slider.h>
#include <TextControl.h>

#include <stdlib.h>

#include <algorithm>

#include "ImageIO.h"
#include "ImageOps.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "EffectWindow"


enum {
	MSG_APPLY	= 'aply',
	MSG_OK		= 'efok'
};

static const float kPreviewSize = 180;


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


// The whole image, scaled down to fit.
class PreviewView : public BView {
public:
	PreviewView(BBitmap* image)
		:
		BView("preview", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
		fImage(image)
	{
		SetExplicitMinSize(BSize(kPreviewSize, kPreviewSize));
		SetExplicitPreferredSize(BSize(kPreviewSize, kPreviewSize));
	}

	void SetImage(BBitmap* image)
	{
		fImage = image;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		FillRect(bounds);
		if (fImage == NULL)
			return;

		BRect imageBounds = fImage->Bounds();
		float scale = std::min(1.0f, std::min(
			(bounds.Width() + 1) / (imageBounds.Width() + 1),
			(bounds.Height() + 1) / (imageBounds.Height() + 1)));
		float width = (imageBounds.Width() + 1) * scale;
		float height = (imageBounds.Height() + 1) * scale;
		BRect target(0, 0, width - 1, height - 1);
		target.OffsetBy(floorf((bounds.Width() - width) / 2),
			floorf((bounds.Height() - height) / 2));

		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fImage, imageBounds, target, B_FILTER_BITMAP_BILINEAR);
		SetDrawingMode(B_OP_COPY);
		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),
			B_DARKEN_2_TINT));
		StrokeRect(target.InsetByCopy(-1, -1));
	}

private:
	BBitmap*	fImage;
};


static const char*
TitleFor(effect_id effect)
{
	switch (effect) {
		case EFFECT_GAUSSIAN_BLUR:
			return B_TRANSLATE("Gaussian blur");
		case EFFECT_SHARPEN:
			return B_TRANSLATE("Sharpen");
		case EFFECT_CUSTOM:
		default:
			return B_TRANSLATE("Custom effect");
	}
}


EffectWindow::EffectWindow(effect_id effect, BBitmap* image,
	BMessenger target, BWindow* parent)
	:
	BWindow(BRect(0, 0, 400, 240), TitleFor(effect), B_TITLED_WINDOW_LOOK,
		parent != NULL ? B_MODAL_SUBSET_WINDOW_FEEL : B_NORMAL_WINDOW_FEEL,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE | B_CLOSE_ON_ESCAPE),
	fEffect(effect),
	fImage(image),
	fTarget(target),
	fIntensity(NULL)
{
	if (parent != NULL)
		AddToSubset(parent);

	fPreview = new PreviewView(fImage);

	BGroupView* settings = new BGroupView(B_VERTICAL);
	if (effect == EFFECT_CUSTOM) {
		BGridLayout* grid = new BGridLayout(B_USE_SMALL_SPACING,
			B_USE_SMALL_SPACING);
		settings->GroupLayout()->AddItem(grid);
		for (int32 i = 0; i < 9; i++) {
			// The identity kernel, so the first Apply changes nothing.
			fKernel[i] = new BTextControl(NULL, NULL, i == 4 ? "1" : "0",
				NULL);
			fKernel[i]->SetExplicitMaxSize(BSize(60, B_SIZE_UNSET));
			grid->AddView(fKernel[i], i % 3, i / 3);
		}
	} else {
		for (int32 i = 0; i < 9; i++)
			fKernel[i] = NULL;
		fIntensity = new BSlider("intensity", B_TRANSLATE("Intensity"), NULL,
			1, 10, B_HORIZONTAL);
		fIntensity->SetHashMarks(B_HASH_MARKS_BOTH);
		fIntensity->SetHashMarkCount(10);
		fIntensity->SetLimitLabels("1", "10");
		fIntensity->SetValue(1);
		settings->AddChild(fIntensity);
	}
	settings->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());

	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"),
		new BMessage(MSG_OK));
	BButton* applyButton = new BButton("apply", B_TRANSLATE("Apply"),
		new BMessage(MSG_APPLY));
	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Cancel"),
		new BMessage(B_QUIT_REQUESTED));

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fPreview)
			.Add(settings)
			.End()
		.AddGroup(B_HORIZONTAL)
			.Add(applyButton)
			.AddGlue()
			.Add(cancelButton)
			.Add(okButton)
			.End();

	okButton->MakeDefault(true);
	if (parent != NULL)
		CenterIn(parent->Frame());
	else
		CenterOnScreen();
}


EffectWindow::~EffectWindow()
{
	delete fImage;
}


void
EffectWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_APPLY:
			_Apply();
			fPreview->SetImage(fImage);
			break;

		case MSG_OK:
		{
			_Apply();
			BMessage result(MSG_EFFECT_RESULT);
			result.AddPointer("bitmap", fImage);
			if (fTarget.SendMessage(&result) == B_OK)
				fImage = NULL;
			Quit();
			break;
		}

		default:
			BWindow::MessageReceived(message);
	}
}


void
EffectWindow::_Apply()
{
	double kernel[9];
	int32 passes = 1;
	switch (fEffect) {
		case EFFECT_GAUSSIAN_BLUR:
			// The blur kernel is normalised, so the original's intensity
			// (a multiplier) had no effect; here it is the number of passes.
			ImageOps::BlurKernel(kernel);
			passes = fIntensity->Value();
			break;
		case EFFECT_SHARPEN:
			ImageOps::SharpenKernel(fIntensity->Value(), kernel);
			break;
		default:
			for (int32 i = 0; i < 9; i++)
				kernel[i] = strtod(fKernel[i]->Text(), NULL);
			break;
	}

	for (int32 pass = 0; pass < passes; pass++) {
		BBitmap* result = ImageIO::Copy(fImage);
		if (result == NULL)
			return;
		ImageOps::Convolve(PixelsOf(fImage), PixelsOf(result), kernel);
		delete fImage;
		fImage = result;
	}
}
