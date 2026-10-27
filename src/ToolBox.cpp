/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "ToolBox.h"

#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ControlLook.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <algorithm>

#include "NumberControl.h"
#include "Settings.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ToolBox"


// #pragma mark - ColorSwatch


ColorSwatch::ColorSwatch(kind type, rgb_color color, float size)
	:
	BView("swatch", B_WILL_DRAW),
	fKind(type),
	fColor(color)
{
	SetExplicitSize(BSize(size - 1, size - 1));
	SetViewColor(B_TRANSPARENT_COLOR);
	switch (type) {
		case SWATCH_PRIMARY:
			SetToolTip(B_TRANSLATE("Primary colour: click to change"));
			break;
		case SWATCH_SECONDARY:
			SetToolTip(B_TRANSLATE("Secondary colour: click to change"));
			break;
		case SWATCH_PALETTE:
			SetToolTip(B_TRANSLATE("Click: primary colour\n"
				"Right-click: secondary colour"));
			break;
		default:
			break;
	}
}


void
ColorSwatch::SetColor(rgb_color color)
{
	if (color == fColor)
		return;
	fColor = color;
	Invalidate();
}


void
ColorSwatch::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	if (fKind == SWATCH_PALETTE) {
		// A plain square, as in the original's palette.
		SetHighColor(fColor);
		FillRect(bounds);
		return;
	}

	// A raised box with the colour inset, like the original's colour
	// buttons.
	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetHighColor(base);
	FillRect(bounds);
	SetHighColor(tint_color(base, B_DARKEN_3_TINT));
	StrokeRect(bounds);
	bounds.InsetBy(1, 1);
	SetHighColor(tint_color(base, B_LIGHTEN_2_TINT));
	StrokeLine(bounds.LeftBottom(), bounds.LeftTop());
	StrokeLine(bounds.RightTop());
	SetHighColor(tint_color(base, B_DARKEN_1_TINT));
	StrokeLine(bounds.RightBottom());
	StrokeLine(bounds.LeftBottom());

	float inset = fKind == SWATCH_DISPLAY ? 2 : 4;
	bounds = Bounds().InsetByCopy(inset, inset);
	SetHighColor(fColor);
	FillRect(bounds);
}


void
ColorSwatch::MouseDown(BPoint where)
{
	int32 buttons = B_PRIMARY_MOUSE_BUTTON;
	if (Window()->CurrentMessage() != NULL)
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
	if ((modifiers() & B_CONTROL_KEY) != 0)
		buttons = B_SECONDARY_MOUSE_BUTTON;

	switch (fKind) {
		case SWATCH_PRIMARY:
		case SWATCH_SECONDARY:
		{
			BMessage edit(MSG_EDIT_COLOR);
			edit.AddInt32("which", fKind == SWATCH_PRIMARY ? 0 : 1);
			edit.AddPoint("where", ConvertToScreen(Bounds().RightTop()));
			be_app->PostMessage(&edit);
			break;
		}
		case SWATCH_PALETTE:
		{
			BMessage setColor(MSG_SET_COLOR);
			setColor.AddInt32("which",
				(buttons & B_SECONDARY_MOUSE_BUTTON) != 0 ? 1 : 0);
			setColor.AddColor("color", fColor);
			be_app->PostMessage(&setColor);
			break;
		}
		default:
			break;
	}
}


void
ColorSwatch::MessageReceived(BMessage* message)
{
	// A colour dropped from another app (or a Haiku colour well).
	const void* data;
	ssize_t size;
	if (message->WasDropped()
		&& (fKind == SWATCH_PRIMARY || fKind == SWATCH_SECONDARY)
		&& message->FindData("RGBColor", B_RGB_COLOR_TYPE, &data, &size)
			== B_OK
		&& size == sizeof(rgb_color)) {
		BMessage setColor(MSG_SET_COLOR);
		setColor.AddInt32("which", fKind == SWATCH_PRIMARY ? 0 : 1);
		setColor.AddColor("color", *(const rgb_color*)data);
		be_app->PostMessage(&setColor);
		return;
	}
	BView::MessageReceived(message);
}


// #pragma mark - ToolBox


static const char* kToolLabels[TOOL_COUNT] = {
	B_TRANSLATE_MARK("Selection"),
	B_TRANSLATE_MARK("Eraser"),
	B_TRANSLATE_MARK("Colour picker"),
	B_TRANSLATE_MARK("Magnifier"),
	B_TRANSLATE_MARK("Pen"),
	B_TRANSLATE_MARK("Line"),
	B_TRANSLATE_MARK("Spray"),
	B_TRANSLATE_MARK("Fill"),
	B_TRANSLATE_MARK("Rectangle"),
	B_TRANSLATE_MARK("Ellipse"),
	B_TRANSLATE_MARK("Curve"),
	B_TRANSLATE_MARK("Text")
};

static const char* kToolIcons[TOOL_COUNT] = {
	"tool-cursor",
	"tool-lastic",
	"tool-pipette",
	"tool-loupe",
	"tool-pencil",
	"tool-line",
	"tool-spray",
	"tool-fill",
	"tool-rectangle",
	"tool-ellipse",
	"tool-curve",
	"tool-text"
};

enum {
	MSG_PEN_FIELD	= 'pnfd',
	MSG_PEN_STEP	= 'pnst'	// "delta": int32
};

static const int32 kMaxPenSize = 50;
static const float kToolButtonSize = 28;


// One button with a minus on its left half and a plus on its right, like
// the original's spin box.
class StepperButton : public BView {
public:
	StepperButton(BMessage* message)
		:
		BView("stepper", B_WILL_DRAW),
		fMessage(message),
		fPressedHalf(0)
	{
		SetExplicitSize(BSize(23, B_SIZE_UNSET));
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	~StepperButton()
	{
		delete fMessage;
	}

	void SetTarget(BHandler* target)
	{
		fTarget = BMessenger(target);
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		rgb_color base = ui_color(B_CONTROL_BACKGROUND_COLOR);
		rgb_color background = ui_color(B_PANEL_BACKGROUND_COLOR);
		be_control_look->DrawButtonFrame(this, bounds, updateRect, base,
			background, fPressedHalf != 0 ? BControlLook::B_ACTIVATED : 0);
		be_control_look->DrawButtonBackground(this, bounds, updateRect, base,
			fPressedHalf != 0 ? BControlLook::B_ACTIVATED : 0);

		SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
		SetPenSize(1);
		float middleY = floorf((bounds.top + bounds.bottom) / 2);
		// A 9-pixel minus and plus with a pixel between them.
		float arm = 4;
		float minusX = floorf((bounds.left + bounds.right) / 2) - 5;
		float plusX = minusX + 10;
		StrokeLine(BPoint(minusX - arm, middleY), BPoint(minusX + arm, middleY));
		StrokeLine(BPoint(plusX - arm, middleY), BPoint(plusX + arm, middleY));
		StrokeLine(BPoint(plusX, middleY - arm), BPoint(plusX, middleY + arm));
	}

	virtual void MouseDown(BPoint where)
	{
		fPressedHalf = where.x < Bounds().Width() / 2 ? -1 : 1;
		Invalidate();
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
		BMessage message(*fMessage);
		message.AddInt32("delta", fPressedHalf);
		fTarget.SendMessage(&message);
	}

	virtual void MouseUp(BPoint where)
	{
		fPressedHalf = 0;
		Invalidate();
	}

private:
	BMessage*	fMessage;
	BMessenger	fTarget;
	int32		fPressedHalf;
};


const char*
ToolBox::ToolLabel(tool_id tool)
{
	return B_TRANSLATE_NOCOLLECT(kToolLabels[tool]);
}


const char*
ToolBox::ToolIconName(tool_id tool)
{
	return kToolIcons[tool];
}


ToolBox::ToolBox()
	:
	BGroupView("toolbox", B_VERTICAL, 0)
{
	// Sizes follow the Qt version on Haiku: 28-pixel buttons, 6 pixels
	// apart, in two columns.
	GroupLayout()->SetInsets(5, 5, 4, 5);

	// The grid has to be in this view's layout before views go into it,
	// or they never become child views.
	BGridLayout* grid;
	BLayoutBuilder::Group<>(this)
		.AddGrid(6, 6)
			.GetLayout(&grid)
			.End()
		.AddGlue();

	for (int32 i = 0; i < TOOL_COUNT; i++) {
		BMessage* message = new BMessage(MSG_SET_TOOL);
		message->AddInt32("tool", i);
		BButton* button = new BButton(kToolIcons[i], NULL, message);
		BBitmap* icon = BTranslationUtils::GetBitmap('PNG ', kToolIcons[i]);
		if (icon != NULL) {
			button->SetIcon(icon);
			delete icon;
		} else
			button->SetLabel(ToolLabel((tool_id)i));
		button->SetBehavior(BButton::B_TOGGLE_BEHAVIOR);
		button->SetExplicitSize(BSize(kToolButtonSize - 1,
			kToolButtonSize - 1));
		button->SetToolTip(ToolLabel((tool_id)i));
		fButtons[i] = button;
		grid->AddView(button, i % 2, i / 2);
	}

	int32 row = (TOOL_COUNT + 1) / 2;
	BSeparatorView* separator = new BSeparatorView(B_HORIZONTAL);
	separator->SetExplicitMinSize(BSize(B_SIZE_UNSET, 4));
	grid->AddView(separator, 0, row++, 2, 1);

	fPrimary = new ColorSwatch(ColorSwatch::SWATCH_PRIMARY,
		gSettings->PrimaryColor(), kToolButtonSize);
	fSecondary = new ColorSwatch(ColorSwatch::SWATCH_SECONDARY,
		gSettings->SecondaryColor(), kToolButtonSize);
	grid->AddView(fPrimary, 0, row);
	grid->AddView(fSecondary, 1, row++);

	fPenField = new NumberControl("penSize", NULL, 1, kMaxPenSize,
		gSettings->PenSize(), new BMessage(MSG_PEN_FIELD));
	fPenField->SetExplicitSize(BSize(35, B_SIZE_UNSET));
	fPenField->SetToolTip(B_TRANSLATE("Pen size"));
	fPenStepper = new StepperButton(new BMessage(MSG_PEN_STEP));
	fPenStepper->SetToolTip(B_TRANSLATE("Pen size"));
	BGroupLayout* penRow = BLayoutBuilder::Group<>(B_HORIZONTAL, 2)
		.Add(fPenField)
		.Add(fPenStepper);
	grid->AddItem(penRow, 0, row, 2, 1);
}


void
ToolBox::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	for (int32 i = 0; i < TOOL_COUNT; i++)
		fButtons[i]->SetTarget(be_app);
	fPenField->SetTarget(this);
	fPenStepper->SetTarget(this);
	Update();
}


void
ToolBox::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_PEN_FIELD:
		case MSG_PEN_STEP:
		{
			int32 size = message->what == MSG_PEN_FIELD
				? fPenField->Number()
				: gSettings->PenSize() + message->GetInt32("delta", 0);
			size = std::min(std::max(size, (int32)1), kMaxPenSize);
			if (size == gSettings->PenSize())
				break;
			BMessage setSize(MSG_PEN_SIZE);
			setSize.AddInt32("size", size);
			be_app->PostMessage(&setSize);
			break;
		}
		default:
			BGroupView::MessageReceived(message);
	}
}


void
ToolBox::Update()
{
	tool_id tool = gSettings->Tool();
	for (int32 i = 0; i < TOOL_COUNT; i++)
		fButtons[i]->SetValue(i == tool ? B_CONTROL_ON : B_CONTROL_OFF);

	fPrimary->SetColor(gSettings->PrimaryColor());
	fSecondary->SetColor(gSettings->SecondaryColor());

	// Leave the field alone while it's being typed into.
	int32 size = gSettings->PenSize();
	if (fPenField->Number() != size && !fPenField->TextView()->IsFocus())
		fPenField->SetNumber(size);
}


// #pragma mark - PaletteBar


// The original's palette, in Qt's colours.
static const rgb_color kPalette[] = {
	{ 0, 0, 0, 255 },		{ 255, 255, 255, 255 },
	{ 255, 0, 0, 255 },		{ 128, 0, 0, 255 },
	{ 0, 255, 0, 255 },		{ 0, 128, 0, 255 },
	{ 0, 0, 255, 255 },		{ 0, 0, 128, 255 },
	{ 0, 255, 255, 255 },	{ 0, 128, 128, 255 },
	{ 255, 0, 255, 255 },	{ 128, 0, 128, 255 },
	{ 255, 255, 0, 255 },	{ 128, 128, 0, 255 },
	{ 160, 160, 164, 255 }
};


PaletteBar::PaletteBar()
	:
	BGroupView("palette", B_HORIZONTAL, 12)
{
	GroupLayout()->SetInsets(6, 7, 6, 6);
	for (const rgb_color& color : kPalette) {
		GroupLayout()->AddView(new ColorSwatch(ColorSwatch::SWATCH_PALETTE,
			color, 20));
	}
	GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
}
