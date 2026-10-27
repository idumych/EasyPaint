/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <GraphicsDefs.h>
#include <SupportDefs.h>


#define APP_NAME		"EasyPaint"
#define APP_SIGNATURE	"application/x-vnd.EasyPaint"
#define APP_VERSION		"0.2.0"


// The tools, in the order of the original's Instruments menu.
enum tool_id {
	TOOL_SELECTION = 0,
	TOOL_ERASER,
	TOOL_COLOR_PICKER,
	TOOL_MAGNIFIER,
	TOOL_PEN,
	TOOL_LINE,
	TOOL_SPRAY,
	TOOL_FILL,
	TOOL_RECTANGLE,
	TOOL_ELLIPSE,
	TOOL_CURVE,
	TOOL_TEXT,

	TOOL_COUNT
};

enum effect_id {
	EFFECT_GRAY = 0,
	EFFECT_NEGATIVE,
	EFFECT_BINARIZATION,
	EFFECT_GAUSSIAN_BLUR,
	EFFECT_GAMMA,
	EFFECT_SHARPEN,
	EFFECT_CUSTOM,

	EFFECT_COUNT
};

enum size_dialog_mode {
	SIZE_NEW_IMAGE = 0,
	SIZE_IMAGE,
	SIZE_CANVAS
};


enum {
	// File menu
	MSG_NEW					= 'new_',
	MSG_OPEN				= 'open',
	MSG_SAVE				= 'save',
	MSG_SAVE_AS				= 'svas',
	MSG_PAGE_SETUP			= 'pgst',
	MSG_PRINT				= 'prnt',

	// Edit menu (B_UNDO, B_REDO, B_CUT, B_COPY, B_PASTE, B_SELECT_ALL too)
	MSG_DELETE_SELECTION	= 'dsel',
	MSG_SETTINGS			= 'stgs',

	// Tools, Effects, Image and View menus
	MSG_SET_TOOL			= 'tool',	// "tool": int32
	MSG_EFFECT				= 'efct',	// "effect": int32
	MSG_IMAGE_SIZE			= 'imsz',
	MSG_CANVAS_SIZE			= 'cvsz',
	MSG_ROTATE				= 'rota',	// "clockwise": bool
	MSG_ZOOM_IN				= 'zmin',
	MSG_ZOOM_OUT			= 'zmot',
	MSG_ZOOM_SET			= 'zmst',	// "zoom": float

	// Replies from the dialog windows
	MSG_SIZE_CHOSEN			= 'szch',	// "mode", "width", "height": int32
	MSG_EFFECT_RESULT		= 'efrs',	// "bitmap": pointer (BBitmap*)
	MSG_TEXT_CHANGED		= 'txch',	// "text": string
	MSG_TEXT_DONE			= 'txdn',	// "cancel": bool
	MSG_TEXT_CLOSE			= 'txcl',	// the canvas closing the text window
	MSG_SETTINGS_CHANGED	= 'stch',

	// Shared drawing state
	MSG_STATE_CHANGED		= 'stat',
	MSG_PEN_SIZE			= 'pnsz',
	MSG_EDIT_COLOR			= 'edcl',	// "which": int32 (0 primary, 1 second.)
	MSG_SET_COLOR			= 'stcl',	// "which": int32, "color": rgb_color

	// From the canvas to its window
	MSG_CANVAS_CHANGED		= 'cvch',
	MSG_POINTER_STATUS		= 'ptst',	// "where": BPoint, "color": rgb_color

	MSG_AUTOSAVE			= 'asav',
	MSG_WINDOW_CLOSED		= 'wcls'
};


// The colour used by the eraser and to fill the hole a moved selection
// leaves behind, as in the original.
static const rgb_color kBackgroundWhite = { 255, 255, 255, 255 };

#endif	// CONSTANTS_H
