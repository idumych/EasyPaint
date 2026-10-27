/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Distributed under the terms of the MIT License.
 *
 * A text field for a whole number in a range. BSpinner would do, but it
 * isn't public API. The message is sent on every edit.
 */
#ifndef NUMBER_CONTROL_H
#define NUMBER_CONTROL_H

#include <TextControl.h>


class NumberControl : public BTextControl {
public:
								NumberControl(const char* name,
									const char* label, int32 minimum,
									int32 maximum, int32 value,
									BMessage* message);

			int32				Number() const;
			void				SetNumber(int32 value);

private:
			int32				fMinimum;
			int32				fMaximum;
};

#endif	// NUMBER_CONTROL_H
