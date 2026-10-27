/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "PaintWindow.h"

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Catalog.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Path.h>
#include <PrintJob.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <StringView.h>

#include <algorithm>

#include "CanvasView.h"
#include "EffectWindow.h"
#include "ImageIO.h"
#include "Settings.h"
#include "SizeWindow.h"
#include "ToolBox.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PaintWindow"


// Draws the image fitted to the page, for BPrintJob.
class PrintView : public BView {
public:
	PrintView(BRect frame, const BBitmap* image)
		:
		BView(frame, "print", B_FOLLOW_NONE, B_WILL_DRAW),
		fImage(image)
	{
	}

	virtual void Draw(BRect updateRect)
	{
		BRect bounds = Bounds();
		BRect imageBounds = fImage->Bounds();
		float scale = std::min((bounds.Width() + 1) / (imageBounds.Width() + 1),
			(bounds.Height() + 1) / (imageBounds.Height() + 1));
		BRect target(0, 0, (imageBounds.Width() + 1) * scale - 1,
			(imageBounds.Height() + 1) * scale - 1);
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fImage, imageBounds, target, B_FILTER_BITMAP_BILINEAR);
	}

private:
	const BBitmap*	fImage;
};


static BMenuItem*
AddItem(BMenu* menu, const char* label, uint32 what, char shortcut = 0,
	uint32 modifiers = 0)
{
	BMenuItem* item = new BMenuItem(label, new BMessage(what), shortcut,
		modifiers);
	menu->AddItem(item);
	return item;
}


PaintWindow::PaintWindow(BRect frame, BBitmap* image, const entry_ref* ref)
	:
	BWindow(frame, B_TRANSLATE("Untitled"), B_DOCUMENT_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS),
	fHasRef(ref != NULL),
	fSavePanel(NULL),
	fQuitAfterSave(false),
	fAutoSaveRunner(NULL),
	fPrintSettings(NULL),
	fTool(gSettings->Tool())
{
	if (ref != NULL)
		fRef = *ref;

	fCanvas = new CanvasView(image);
	fScrollView = new BScrollView("scroller", fCanvas, 0, true, true,
		B_NO_BORDER);
	fScrollView->SetExplicitMinSize(BSize(200, 150));
	fToolBox = new ToolBox();
	fPalette = new PaletteBar();

	_BuildMenuBar();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fMenuBar)
		.AddGroup(B_HORIZONTAL, 0)
			.Add(fToolBox)
			.Add(new BSeparatorView(B_VERTICAL))
			.Add(fScrollView)
			.End()
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(fPalette)
		.Add(_BuildStatusBar());

	fCanvas->MakeFocus(true);
	_UpdateTitle();
	_UpdateMenus();
	_UpdateStatus();
	_UpdateState();
	_StartAutoSave();
}


PaintWindow::~PaintWindow()
{
	delete fSavePanel;
	delete fAutoSaveRunner;
	delete fPrintSettings;
}


bool
PaintWindow::QuitRequested()
{
	if (fCanvas->IsModified()) {
		BString text;
		text.SetToFormat(B_TRANSLATE("Save changes to \"%s\" before closing?"),
			Title());
		BAlert* alert = new BAlert(B_TRANSLATE("Close"), text.String(),
			B_TRANSLATE("Cancel"), B_TRANSLATE("Don't save"),
			B_TRANSLATE("Save"), B_WIDTH_AS_USUAL, B_OFFSET_SPACING,
			B_WARNING_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		switch (alert->Go()) {
			case 0:
				return false;
			case 2:
				if (!fHasRef) {
					fQuitAfterSave = true;
					_SaveAs();
					return false;
				}
				_Save();
				if (fCanvas->IsModified())
					return false;
				break;
		}
	}

	gSettings->SetWindowFrame(Frame());
	be_app->PostMessage(MSG_WINDOW_CLOSED);
	return true;
}


void
PaintWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case MSG_SAVE:
			_Save();
			break;
		case MSG_SAVE_AS:
			_SaveAs();
			break;
		case B_SAVE_REQUESTED:
		{
			entry_ref directory;
			const char* name;
			if (message->FindRef("directory", &directory) == B_OK
				&& message->FindString("name", &name) == B_OK
				&& _SaveTo(directory, name) && fQuitAfterSave) {
				PostMessage(B_QUIT_REQUESTED);
			}
			fQuitAfterSave = false;
			break;
		}
		case B_CANCEL:
			// The save panel was closed without saving.
			fQuitAfterSave = false;
			break;
		case MSG_PAGE_SETUP:
			_PageSetup();
			break;
		case MSG_PRINT:
			_Print();
			break;

		case B_UNDO:
			fCanvas->Undo();
			break;
		case B_REDO:
			fCanvas->Redo();
			break;
		case B_CUT:
			fCanvas->Cut();
			break;
		case B_COPY:
			fCanvas->Copy();
			break;
		case B_PASTE:
			_SwitchTool(TOOL_SELECTION);
			fCanvas->Paste();
			break;
		case B_SELECT_ALL:
			_SwitchTool(TOOL_SELECTION);
			fCanvas->SelectAll();
			break;
		case MSG_DELETE_SELECTION:
			fCanvas->DeleteSelection();
			break;

		case MSG_EFFECT:
		{
			effect_id effect = (effect_id)message->GetInt32("effect",
				EFFECT_GRAY);
			if (effect == EFFECT_GAUSSIAN_BLUR || effect == EFFECT_SHARPEN
				|| effect == EFFECT_CUSTOM) {
				_OpenEffect(effect);
			} else
				fCanvas->ApplySimpleEffect(effect);
			break;
		}
		case MSG_EFFECT_RESULT:
		{
			BBitmap* bitmap;
			if (message->FindPointer("bitmap", (void**)&bitmap) == B_OK)
				fCanvas->ReplaceImage(bitmap, true);
			break;
		}

		case MSG_IMAGE_SIZE:
		case MSG_CANVAS_SIZE:
			fCanvas->FinishTools();
			(new SizeWindow(message->what == MSG_IMAGE_SIZE
					? SIZE_IMAGE : SIZE_CANVAS,
				fCanvas->ImageWidth(), fCanvas->ImageHeight(),
				BMessenger(this), this))->Show();
			break;
		case MSG_SIZE_CHOSEN:
		{
			int32 width = message->GetInt32("width", 1);
			int32 height = message->GetInt32("height", 1);
			if (message->GetInt32("mode", SIZE_IMAGE) == SIZE_IMAGE)
				fCanvas->ResizeImage(width, height);
			else
				fCanvas->ResizeCanvas(width, height);
			break;
		}
		case MSG_ROTATE:
			fCanvas->Rotate(message->GetBool("clockwise", true));
			break;

		case MSG_ZOOM_IN:
			fCanvas->ZoomIn();
			break;
		case MSG_ZOOM_OUT:
			fCanvas->ZoomOut();
			break;
		case MSG_ZOOM_SET:
			fCanvas->SetZoom(message->GetFloat("zoom", 1.0f));
			break;

		case MSG_CANVAS_CHANGED:
			_UpdateMenus();
			_UpdateStatus();
			break;
		case MSG_POINTER_STATUS:
		{
			BPoint where;
			BString position;
			if (message->FindPoint("where", &where) == B_OK)
				position.SetToFormat("%d,%d", (int)where.x, (int)where.y);
			fPositionView->SetText(position.String());

			rgb_color color;
			if (message->FindColor("color", &color) == B_OK) {
				BString text;
				text.SetToFormat("RGB: %d, %d, %d", color.red, color.green,
					color.blue);
				fColorView->SetText(text.String());
				fColorSwatch->SetColor(color);
				if (fColorSwatch->IsHidden())
					fColorSwatch->Show();
			} else if (!fColorSwatch->IsHidden()) {
				fColorSwatch->Hide();
				fColorView->SetText("");
			}
			break;
		}

		case MSG_STATE_CHANGED:
			_UpdateState();
			break;
		case MSG_SETTINGS_CHANGED:
			_StartAutoSave();
			break;
		case MSG_AUTOSAVE:
			if (gSettings->AutoSave() && fHasRef && fCanvas->IsModified()
				&& !fCanvas->IsBusy()) {
				_Save();
			}
			break;

		case B_SIMPLE_DATA:
		case B_REFS_RECEIVED:
			// Image files dropped on the window open in new windows.
			if (message->HasRef("refs")) {
				BMessage refs(*message);
				refs.what = B_REFS_RECEIVED;
				be_app->PostMessage(&refs);
				break;
			}
			BWindow::MessageReceived(message);
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
PaintWindow::MenusBeginning()
{
	_UpdateMenus();
	BWindow::MenusBeginning();
}


// #pragma mark - Building


void
PaintWindow::_BuildMenuBar()
{
	fMenuBar = new BMenuBar("menu");

	BMenu* menu = new BMenu(B_TRANSLATE("File"));
	BMenuItem* item = AddItem(menu, B_TRANSLATE("New" B_UTF8_ELLIPSIS),
		MSG_NEW, 'N');
	item->SetTarget(be_app);
	item = AddItem(menu, B_TRANSLATE("Open" B_UTF8_ELLIPSIS), MSG_OPEN, 'O');
	item->SetTarget(be_app);
	menu->AddSeparatorItem();
	AddItem(menu, B_TRANSLATE("Save"), MSG_SAVE, 'S');
	AddItem(menu, B_TRANSLATE("Save as" B_UTF8_ELLIPSIS), MSG_SAVE_AS, 'S',
		B_SHIFT_KEY);
	menu->AddSeparatorItem();
	AddItem(menu, B_TRANSLATE("Page setup" B_UTF8_ELLIPSIS), MSG_PAGE_SETUP);
	AddItem(menu, B_TRANSLATE("Print" B_UTF8_ELLIPSIS), MSG_PRINT, 'P');
	menu->AddSeparatorItem();
	AddItem(menu, B_TRANSLATE("Close"), B_QUIT_REQUESTED, 'W');
	item = AddItem(menu, B_TRANSLATE("Quit"), B_QUIT_REQUESTED, 'Q');
	item->SetTarget(be_app);
	fMenuBar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Edit"));
	fUndoItem = AddItem(menu, B_TRANSLATE("Undo"), B_UNDO, 'Z');
	fRedoItem = AddItem(menu, B_TRANSLATE("Redo"), B_REDO, 'Z', B_SHIFT_KEY);
	menu->AddSeparatorItem();
	fCutItem = AddItem(menu, B_TRANSLATE("Cut"), B_CUT, 'X');
	fCopyItem = AddItem(menu, B_TRANSLATE("Copy"), B_COPY, 'C');
	AddItem(menu, B_TRANSLATE("Paste"), B_PASTE, 'V');
	fDeleteItem = AddItem(menu, B_TRANSLATE("Delete"), MSG_DELETE_SELECTION);
	AddItem(menu, B_TRANSLATE("Select all"), B_SELECT_ALL, 'A');
	menu->AddSeparatorItem();
	item = AddItem(menu, B_TRANSLATE("Settings" B_UTF8_ELLIPSIS),
		MSG_SETTINGS, ',');
	item->SetTarget(be_app);
	fMenuBar->AddItem(menu);

	// Alt+1 to Alt+0 select the first ten tools, as Ctrl+1 to Ctrl+0 did in
	// the original.
	menu = new BMenu(B_TRANSLATE("Tools"));
	menu->SetRadioMode(true);
	for (int32 i = 0; i < TOOL_COUNT; i++) {
		BMessage* message = new BMessage(MSG_SET_TOOL);
		message->AddInt32("tool", i);
		char shortcut = i < 9 ? '1' + i : i == 9 ? '0' : 0;
		fToolItems[i] = new BMenuItem(ToolBox::ToolLabel((tool_id)i),
			message, shortcut);
		fToolItems[i]->SetTarget(be_app);
		menu->AddItem(fToolItems[i]);
	}
	fMenuBar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Effects"));
	static const struct {
		const char*	label;
		effect_id	effect;
	} kEffects[] = {
		{ B_TRANSLATE_MARK("Gray"), EFFECT_GRAY },
		{ B_TRANSLATE_MARK("Negative"), EFFECT_NEGATIVE },
		{ B_TRANSLATE_MARK("Binarization"), EFFECT_BINARIZATION },
		{ B_TRANSLATE_MARK("Gaussian blur" B_UTF8_ELLIPSIS),
			EFFECT_GAUSSIAN_BLUR },
		{ B_TRANSLATE_MARK("Gamma"), EFFECT_GAMMA },
		{ B_TRANSLATE_MARK("Sharpen" B_UTF8_ELLIPSIS), EFFECT_SHARPEN },
		{ B_TRANSLATE_MARK("Custom" B_UTF8_ELLIPSIS), EFFECT_CUSTOM },
	};
	for (const auto& effect : kEffects) {
		BMessage* message = new BMessage(MSG_EFFECT);
		message->AddInt32("effect", effect.effect);
		menu->AddItem(new BMenuItem(B_TRANSLATE_NOCOLLECT(effect.label),
			message));
	}
	fMenuBar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Image"));
	AddItem(menu, B_TRANSLATE("Image size" B_UTF8_ELLIPSIS), MSG_IMAGE_SIZE);
	AddItem(menu, B_TRANSLATE("Canvas size" B_UTF8_ELLIPSIS),
		MSG_CANVAS_SIZE);
	menu->AddSeparatorItem();
	BMessage* rotate = new BMessage(MSG_ROTATE);
	rotate->AddBool("clockwise", true);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Rotate clockwise"), rotate,
		'R'));
	rotate = new BMessage(MSG_ROTATE);
	rotate->AddBool("clockwise", false);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Rotate counter-clockwise"),
		rotate, 'R', B_SHIFT_KEY));
	fMenuBar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("View"));
	AddItem(menu, B_TRANSLATE("Zoom in"), MSG_ZOOM_IN, '+');
	AddItem(menu, B_TRANSLATE("Zoom out"), MSG_ZOOM_OUT, '-');
	menu->AddSeparatorItem();
	static const float kZooms[] = { 0.25f, 0.5f, 1, 2, 4, 8, 16 };
	for (float zoom : kZooms) {
		BMessage* message = new BMessage(MSG_ZOOM_SET);
		message->AddFloat("zoom", zoom);
		BString label;
		label.SetToFormat("%g%%", zoom * 100);
		menu->AddItem(new BMenuItem(label.String(), message));
	}
	fMenuBar->AddItem(menu);

	menu = new BMenu(B_TRANSLATE("Help"));
	item = AddItem(menu, B_TRANSLATE("About EasyPaint" B_UTF8_ELLIPSIS),
		B_ABOUT_REQUESTED);
	item->SetTarget(be_app);
	fMenuBar->AddItem(menu);
}


BView*
PaintWindow::_BuildStatusBar()
{
	fSizeView = new BStringView("size", "");
	fPositionView = new BStringView("position", "");
	fZoomView = new BStringView("zoom", "");
	fColorSwatch = new ColorSwatch(ColorSwatch::SWATCH_DISPLAY,
		make_color(0, 0, 0), 12);
	fColorView = new BStringView("color", "");

	fSizeView->SetExplicitMinSize(BSize(
		be_plain_font->StringWidth("0000 x 0000"), B_SIZE_UNSET));
	fPositionView->SetExplicitMinSize(BSize(
		be_plain_font->StringWidth("0000,0000"), B_SIZE_UNSET));

	BGroupView* statusBar = new BGroupView(B_HORIZONTAL, 6);
	BLayoutBuilder::Group<>(statusBar)
		.SetInsets(6, 0, 6, 5)
		.Add(fSizeView)
		.Add(fPositionView)
		.Add(fZoomView)
		.AddGlue()
		.Add(fColorSwatch)
		.Add(fColorView)
		// Room for the document window's resize corner.
		.AddStrut(B_V_SCROLL_BAR_WIDTH);
	fColorSwatch->Hide();
	return statusBar;
}


// #pragma mark - Saving and printing


void
PaintWindow::_Save()
{
	if (!fHasRef) {
		_SaveAs();
		return;
	}
	BEntry entry(&fRef);
	BEntry parent;
	entry_ref directory;
	if (entry.GetParent(&parent) != B_OK
		|| parent.GetRef(&directory) != B_OK) {
		_SaveAs();
		return;
	}
	_SaveTo(directory, fRef.name);
}


void
PaintWindow::_SaveAs()
{
	if (fSavePanel == NULL) {
		BMessenger target(this);
		fSavePanel = new BFilePanel(B_SAVE_PANEL, &target, NULL, 0, false);
	}

	if (fHasRef) {
		BEntry entry(&fRef);
		BEntry parent;
		if (entry.GetParent(&parent) == B_OK)
			fSavePanel->SetPanelDirectory(&parent);
		fSavePanel->SetSaveText(fRef.name);
	} else {
		BString name(B_TRANSLATE("Untitled image"));
		name << ".png";
		fSavePanel->SetSaveText(name.String());
	}
	fSavePanel->Window()->SetTitle(B_TRANSLATE("EasyPaint: Save image"));
	fSavePanel->Show();
}


bool
PaintWindow::_SaveTo(const entry_ref& directory, const char* name)
{
	fCanvas->FinishTools();
	BString error;
	status_t status = ImageIO::Save(fCanvas->Image(), directory, name, &error);
	if (status != B_OK) {
		BString text;
		text.SetToFormat(B_TRANSLATE("Can't save \"%s\": %s"), name,
			error.String());
		BAlert* alert = new BAlert(B_TRANSLATE("Error saving file"),
			text.String(), B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL,
			B_STOP_ALERT);
		alert->Go(NULL);
		return false;
	}

	BDirectory dir(&directory);
	BEntry entry(&dir, name);
	if (entry.GetRef(&fRef) == B_OK)
		fHasRef = true;
	fCanvas->SetModified(false);
	_UpdateTitle();
	return true;
}


void
PaintWindow::_PageSetup()
{
	BPrintJob job(Title());
	if (fPrintSettings != NULL)
		job.SetSettings(new BMessage(*fPrintSettings));
	if (job.ConfigPage() == B_OK) {
		delete fPrintSettings;
		fPrintSettings = job.Settings();
	}
}


void
PaintWindow::_Print()
{
	BPrintJob job(Title());
	if (fPrintSettings == NULL) {
		if (job.ConfigPage() != B_OK)
			return;
		fPrintSettings = job.Settings();
	} else
		job.SetSettings(new BMessage(*fPrintSettings));
	if (job.ConfigJob() != B_OK)
		return;
	delete fPrintSettings;
	fPrintSettings = job.Settings();

	fCanvas->FinishTools();
	BRect printable = job.PrintableRect();
	printable.OffsetTo(B_ORIGIN);

	// BPrintJob draws views that are in a window; this one stays hidden.
	PrintView* view = new PrintView(printable, fCanvas->Image());
	fCanvas->AddChild(view);
	view->Hide();

	job.BeginJob();
	job.DrawView(view, view->Bounds(), B_ORIGIN);
	job.SpoolPage();
	job.CommitJob();

	fCanvas->RemoveChild(view);
	delete view;
}


void
PaintWindow::_OpenEffect(effect_id effect)
{
	fCanvas->FinishTools();
	BBitmap* copy = fCanvas->CopyImage();
	if (copy == NULL)
		return;
	(new EffectWindow(effect, copy, BMessenger(this), this))->Show();
}


// #pragma mark - Updating


void
PaintWindow::_UpdateTitle()
{
	SetTitle(fHasRef ? fRef.name : B_TRANSLATE("Untitled"));
}


void
PaintWindow::_UpdateMenus()
{
	fUndoItem->SetEnabled(fCanvas->CanUndo());
	fRedoItem->SetEnabled(fCanvas->CanRedo());
	bool selection = fCanvas->HasSelection();
	fCutItem->SetEnabled(selection);
	fCopyItem->SetEnabled(selection);
	fDeleteItem->SetEnabled(selection);
}


void
PaintWindow::_UpdateStatus()
{
	BString size;
	size.SetToFormat("%" B_PRId32 " x %" B_PRId32, fCanvas->ImageWidth(),
		fCanvas->ImageHeight());
	fSizeView->SetText(size.String());

	BString zoom;
	zoom.SetToFormat("%g%%", fCanvas->Zoom() * 100);
	fZoomView->SetText(zoom.String());
}


void
PaintWindow::_UpdateState()
{
	fToolBox->Update();
	tool_id tool = gSettings->Tool();
	fToolItems[tool]->SetMarked(true);
	if (tool != fTool) {
		fTool = tool;
		fCanvas->ToolChanged();
		if (tool != TOOL_COLOR_PICKER && !fColorSwatch->IsHidden()) {
			fColorSwatch->Hide();
			fColorView->SetText("");
		}
	} else
		fCanvas->UpdateCursor();
}


// Switches tools from this window at once, so the selection that Paste
// or Select all is about to make isn't finished when the application's
// MSG_STATE_CHANGED arrives.
void
PaintWindow::_SwitchTool(tool_id tool)
{
	if (gSettings->Tool() == tool)
		return;
	gSettings->SetTool(tool);
	_UpdateState();
	BMessage setTool(MSG_SET_TOOL);
	setTool.AddInt32("tool", tool);
	be_app->PostMessage(&setTool);
}


void
PaintWindow::_StartAutoSave()
{
	delete fAutoSaveRunner;
	fAutoSaveRunner = NULL;
	if (!gSettings->AutoSave())
		return;
	BMessage autoSave(MSG_AUTOSAVE);
	fAutoSaveRunner = new BMessageRunner(BMessenger(this), &autoSave,
		(bigtime_t)gSettings->AutoSaveInterval() * 1000000);
}
