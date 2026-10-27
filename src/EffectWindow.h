/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * Settings and a preview for the effects that have them: Gaussian blur,
 * sharpen, and a custom 3x3 convolution. As in the original, "Apply"
 * applies the effect to the preview again, and "OK" applies it once more
 * and hands the result (MSG_EFFECT_RESULT) to the target.
 */
#ifndef EFFECT_WINDOW_H
#define EFFECT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include "Constants.h"


class BBitmap;
class BSlider;
class BTextControl;
class PreviewView;


class EffectWindow : public BWindow {
public:
								// Takes ownership of image.
								EffectWindow(effect_id effect,
									BBitmap* image, BMessenger target,
									BWindow* parent);
	virtual						~EffectWindow();

	virtual	void				MessageReceived(BMessage* message);

private:
			void				_Apply();

			effect_id			fEffect;
			BBitmap*			fImage;
			BMessenger			fTarget;
			PreviewView*		fPreview;
			BSlider*			fIntensity;
			BTextControl*		fKernel[9];
};

#endif	// EFFECT_WINDOW_H
