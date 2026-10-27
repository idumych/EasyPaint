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
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <StringView.h>
#include <TranslationUtils.h>
#include <Window.h>

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
	SetExplicitSize(BSize(size, size));
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
	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetHighColor(tint_color(base, B_DARKEN_2_TINT));
	StrokeLine(bounds.LeftBottom(), bounds.LeftTop());
	StrokeLine(bounds.RightTop());
	SetHighColor(tint_color(base, B_LIGHTEN_2_TINT));
	StrokeLine(bounds.RightBottom());
	StrokeLine(bounds.LeftBottom());
	bounds.InsetBy(1, 1);
	SetHighColor(0, 0, 0);
	StrokeRect(bounds);
	bounds.InsetBy(1, 1);
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

static const int32 kPenSizes[] = {
	1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 48
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
	BGroupView("toolbox", B_VERTICAL, B_USE_SMALL_SPACING)
{
	GroupLayout()->SetInsets(B_USE_SMALL_SPACING);

	// The grid has to be in this view's layout before buttons go into it,
	// or they never become child views.
	BGridLayout* grid;
	BLayoutBuilder::Group<>(this)
		.AddGrid(2, 2)
			.GetLayout(&grid)
			.End();
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
		button->SetFlat(true);
		button->SetToolTip(ToolLabel((tool_id)i));
		fButtons[i] = button;
		grid->AddView(button, i % 2, i / 2);
	}

	fPrimary = new ColorSwatch(ColorSwatch::SWATCH_PRIMARY,
		gSettings->PrimaryColor(), 28);
	fSecondary = new ColorSwatch(ColorSwatch::SWATCH_SECONDARY,
		gSettings->SecondaryColor(), 28);

	fPenMenu = new BPopUpMenu("pen");
	for (int32 size : kPenSizes) {
		BMessage* message = new BMessage(MSG_PEN_SIZE);
		message->AddInt32("size", size);
		BString label;
		label << size;
		fPenMenu->AddItem(new BMenuItem(label.String(), message));
	}
	fPenField = new BMenuField("penSize", NULL, fPenMenu);
	fPenField->SetToolTip(B_TRANSLATE("Pen size"));

	BLayoutBuilder::Group<>(this)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(fPrimary)
			.Add(fSecondary)
			.AddGlue()
			.End()
		.Add(new BStringView("penLabel", B_TRANSLATE("Pen size:")))
		.Add(fPenField)
		.AddGlue();
}


void
ToolBox::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	for (int32 i = 0; i < TOOL_COUNT; i++)
		fButtons[i]->SetTarget(be_app);
	fPenMenu->SetTargetForItems(be_app);
	Update();
}


void
ToolBox::Update()
{
	tool_id tool = gSettings->Tool();
	for (int32 i = 0; i < TOOL_COUNT; i++)
		fButtons[i]->SetValue(i == tool ? B_CONTROL_ON : B_CONTROL_OFF);

	fPrimary->SetColor(gSettings->PrimaryColor());
	fSecondary->SetColor(gSettings->SecondaryColor());

	BString label;
	label << gSettings->PenSize();
	BMenuItem* item = fPenMenu->FindItem(label.String());
	if (item != NULL)
		item->SetMarked(true);
	else
		fPenField->MenuItem()->SetLabel(label.String());
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
	BGroupView("palette", B_HORIZONTAL, 2)
{
	GroupLayout()->SetInsets(B_USE_SMALL_SPACING, 2, B_USE_SMALL_SPACING, 2);
	for (const rgb_color& color : kPalette) {
		GroupLayout()->AddView(new ColorSwatch(ColorSwatch::SWATCH_PALETTE,
			color, 20));
	}
	GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
}
