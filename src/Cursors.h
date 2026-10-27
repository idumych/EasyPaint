/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The mouse pointers over the canvas: system pointers, the original's
 * tool pointers (stored as PNG resources), and the round brush outline
 * for the pen and the eraser. They are made on first use and kept.
 */
#ifndef CURSORS_H
#define CURSORS_H

#include <Cursor.h>
#include <GraphicsDefs.h>


namespace Cursors {

const BCursor*	System(BCursorID id);
const BCursor*	FromResource(const char* name);
const BCursor*	Brush(float diameter, rgb_color fill);

}	// namespace Cursors

#endif	// CURSORS_H
