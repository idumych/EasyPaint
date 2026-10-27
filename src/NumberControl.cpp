/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Distributed under the terms of the MIT License.
 */
#include "NumberControl.h"

#include <String.h>

#include <ctype.h>
#include <stdlib.h>

#include <algorithm>


NumberControl::NumberControl(const char* name, const char* label,
	int32 minimum, int32 maximum, int32 value, BMessage* message)
	:
	BTextControl(name, label, "", message),
	fMinimum(minimum),
	fMaximum(maximum)
{
	SetModificationMessage(new BMessage(*message));
	BTextView* textView = TextView();
	for (uint32 c = 0; c < 256; c++) {
		if (!isdigit(c))
			textView->DisallowChar(c);
	}
	BString maxText;
	maxText << maximum;
	textView->SetMaxBytes(maxText.Length());
	SetExplicitMaxSize(BSize(StringWidth(maxText.String()) * 2
		+ StringWidth(label != NULL ? label : "") + 30, B_SIZE_UNSET));

	SetNumber(value);
}


int32
NumberControl::Number() const
{
	int32 value = atol(Text());
	return std::min(std::max(value, fMinimum), fMaximum);
}


void
NumberControl::SetNumber(int32 value)
{
	BString text;
	text << std::min(std::max(value, fMinimum), fMaximum);
	// SetText() would send the modification message; callers that set
	// values in response to one guard against that themselves.
	SetText(text.String());
}
