/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 */
#include "ImageOps.h"

#include <math.h>
#include <string.h>

#include <vector>


namespace ImageOps {

static inline uint8_t
Clamp(double value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return (uint8_t)lround(value);
}


void
Gray(const Pixels& image)
{
	for (int32_t y = 0; y < image.height; y++) {
		for (int32_t x = 0; x < image.width; x++) {
			uint32_t& p = image.At(x, y);
			uint8_t v = (uint8_t)(0.299 * Red(p) + 0.587 * Green(p)
				+ 0.114 * Blue(p));
			p = MakePixel(v, v, v, Alpha(p));
		}
	}
}


void
Negative(const Pixels& image)
{
	for (int32_t y = 0; y < image.height; y++) {
		for (int32_t x = 0; x < image.width; x++)
			image.At(x, y) ^= 0x00ffffff;
	}
}


void
Binarize(const Pixels& image, int threshold)
{
	for (int32_t y = 0; y < image.height; y++) {
		for (int32_t x = 0; x < image.width; x++) {
			uint32_t& p = image.At(x, y);
			int luma = (int)(0.299 * Red(p) + 0.587 * Green(p)
				+ 0.114 * Blue(p));
			uint8_t v = luma >= threshold ? 255 : 0;
			p = MakePixel(v, v, v, Alpha(p));
		}
	}
}


void
Gamma(const Pixels& image, double exponent)
{
	uint8_t table[256];
	for (int i = 0; i < 256; i++)
		table[i] = Clamp(255 * pow(i / 255.0, exponent));

	for (int32_t y = 0; y < image.height; y++) {
		for (int32_t x = 0; x < image.width; x++) {
			uint32_t& p = image.At(x, y);
			p = MakePixel(table[Red(p)], table[Green(p)], table[Blue(p)],
				Alpha(p));
		}
	}
}


void
Convolve(const Pixels& src, const Pixels& dst, const double kernel[9])
{
	double total = 0;
	for (int i = 0; i < 9; i++)
		total += kernel[i];
	if (total == 0)
		total = 1;

	for (int32_t y = 0; y < src.height; y++) {
		for (int32_t x = 0; x < src.width; x++) {
			if (y < 2 || x < 2 || y >= src.height - 2 || x >= src.width - 2) {
				dst.At(x, y) = src.At(x, y);
				continue;
			}
			double red = 0, green = 0, blue = 0;
			for (int r = -1; r <= 1; r++) {
				for (int c = -1; c <= 1; c++) {
					double k = kernel[(r + 1) * 3 + (c + 1)];
					uint32_t p = src.At(x + c, y + r);
					red += Red(p) * k;
					green += Green(p) * k;
					blue += Blue(p) * k;
				}
			}
			dst.At(x, y) = MakePixel(Clamp(red / total),
				Clamp(green / total), Clamp(blue / total),
				Alpha(src.At(x, y)));
		}
	}
}


void
BlurKernel(double kernel[9])
{
	static const double kBlur[9] = { 1, 2, 1, 2, 4, 2, 1, 2, 1 };
	memcpy(kernel, kBlur, sizeof(kBlur));
}


void
SharpenKernel(int intensity, double kernel[9])
{
	double i = intensity;
	double values[9] = {
		0, -i, 0,
		-i, i * 4 + 1, -i,
		0, -i, 0
	};
	memcpy(kernel, values, sizeof(values));
}


int64_t
FloodFill(const Pixels& image, int32_t x, int32_t y, uint32_t color)
{
	if (!image.Contains(x, y))
		return 0;
	uint32_t target = image.At(x, y);
	if (target == color)
		return 0;

	// Scanline fill with an explicit stack, so a large area can't overflow
	// the thread's stack the way the original's recursion could.
	int64_t count = 0;
	std::vector<int32_t> stack;
	stack.push_back(x);
	stack.push_back(y);
	while (!stack.empty()) {
		int32_t py = stack.back();
		stack.pop_back();
		int32_t px = stack.back();
		stack.pop_back();
		if (image.At(px, py) != target)
			continue;

		int32_t left = px;
		while (left > 0 && image.At(left - 1, py) == target)
			left--;
		int32_t right = px;
		while (right < image.width - 1 && image.At(right + 1, py) == target)
			right++;

		for (int32_t i = left; i <= right; i++) {
			image.At(i, py) = color;
			count++;
		}
		for (int dy = -1; dy <= 1; dy += 2) {
			int32_t ny = py + dy;
			if (ny < 0 || ny >= image.height)
				continue;
			bool inRun = false;
			for (int32_t i = left; i <= right; i++) {
				bool match = image.At(i, ny) == target;
				if (match && !inRun) {
					stack.push_back(i);
					stack.push_back(ny);
				}
				inRun = match;
			}
		}
	}
	return count;
}


void
Scale(const Pixels& src, const Pixels& dst)
{
	if (src.width <= 0 || src.height <= 0)
		return;
	for (int32_t y = 0; y < dst.height; y++) {
		int32_t sy = (int32_t)((int64_t)y * src.height / dst.height);
		for (int32_t x = 0; x < dst.width; x++) {
			int32_t sx = (int32_t)((int64_t)x * src.width / dst.width);
			dst.At(x, y) = src.At(sx, sy);
		}
	}
}


void
Rotate90(const Pixels& src, const Pixels& dst, bool clockwise)
{
	for (int32_t y = 0; y < src.height; y++) {
		for (int32_t x = 0; x < src.width; x++) {
			if (clockwise)
				dst.At(src.height - 1 - y, x) = src.At(x, y);
			else
				dst.At(y, src.width - 1 - x) = src.At(x, y);
		}
	}
}


void
FillRect(const Pixels& image, int32_t x, int32_t y, int32_t width,
	int32_t height, uint32_t color)
{
	int32_t left = x < 0 ? 0 : x;
	int32_t top = y < 0 ? 0 : y;
	int32_t right = x + width > image.width ? image.width : x + width;
	int32_t bottom = y + height > image.height ? image.height : y + height;
	for (int32_t row = top; row < bottom; row++) {
		for (int32_t col = left; col < right; col++)
			image.At(col, row) = color;
	}
}


void
Blit(const Pixels& src, int32_t srcX, int32_t srcY, int32_t width,
	int32_t height, const Pixels& dst, int32_t dstX, int32_t dstY)
{
	// Clip against the source, then the destination.
	if (srcX < 0) { width += srcX; dstX -= srcX; srcX = 0; }
	if (srcY < 0) { height += srcY; dstY -= srcY; srcY = 0; }
	if (dstX < 0) { width += dstX; srcX -= dstX; dstX = 0; }
	if (dstY < 0) { height += dstY; srcY -= dstY; dstY = 0; }
	if (srcX + width > src.width)
		width = src.width - srcX;
	if (srcY + height > src.height)
		height = src.height - srcY;
	if (dstX + width > dst.width)
		width = dst.width - dstX;
	if (dstY + height > dst.height)
		height = dst.height - dstY;
	if (width <= 0 || height <= 0)
		return;

	for (int32_t row = 0; row < height; row++) {
		memmove(&dst.At(dstX, dstY + row), &src.At(srcX, srcY + row),
			width * sizeof(uint32_t));
	}
}


void
SprayPattern(double penSize, int (*random)(int bound), SprayDot dots[12])
{
	double spread = sqrt(penSize);
	for (int i = 0; i < 12; i++) {
		int bound, offset;
		if (i < 4) {
			bound = 5;
			offset = 2;
		} else if (i < 8) {
			bound = 10;
			offset = 4;
		} else {
			bound = 15;
			offset = 7;
		}
		dots[i].x = (int32_t)((random(bound) - offset) * spread);
		dots[i].y = (int32_t)((random(bound) - offset) * spread);
	}
}

}	// namespace ImageOps
