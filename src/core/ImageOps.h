/*
 * EasyPaint for Haiku - a simple bitmap graphics editor
 * Copyright (c) 2012 EasyPaint <https://github.com/Gr1N/EasyPaint>
 * Distributed under the terms of the MIT License.
 *
 * The pixel work behind the effects, the fill tool and the Image menu.
 * It has no Haiku headers, so the unit tests build and run on any host.
 *
 * Pixels are 32-bit 0xAARRGGBB values in native byte order, which is the
 * memory layout of a B_RGBA32 BBitmap on a little-endian machine.
 */
#ifndef IMAGE_OPS_H
#define IMAGE_OPS_H

#include <stdint.h>


namespace ImageOps {

struct Pixels {
	uint32_t*	bits;
	int32_t		width;
	int32_t		height;
	int32_t		stride;		// in pixels, not bytes

	uint32_t&	At(int32_t x, int32_t y) const
					{ return bits[y * stride + x]; }
	bool		Contains(int32_t x, int32_t y) const
					{ return x >= 0 && y >= 0 && x < width && y < height; }
};


inline uint8_t Alpha(uint32_t p) { return p >> 24; }
inline uint8_t Red(uint32_t p) { return (p >> 16) & 0xff; }
inline uint8_t Green(uint32_t p) { return (p >> 8) & 0xff; }
inline uint8_t Blue(uint32_t p) { return p & 0xff; }

inline uint32_t
MakePixel(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
{
	return ((uint32_t)alpha << 24) | ((uint32_t)red << 16)
		| ((uint32_t)green << 8) | blue;
}


// Effects (Effects menu). Alpha is kept as it is.
void		Gray(const Pixels& image);
void		Negative(const Pixels& image);
void		Binarize(const Pixels& image, int threshold);
void		Gamma(const Pixels& image, double exponent);

// A 3x3 convolution of src into dst (same size). Like the original
// EasyPaint, the kernel is normalised by its sum unless that is 0, and a
// two-pixel border is copied unchanged.
void		Convolve(const Pixels& src, const Pixels& dst,
				const double kernel[9]);

// Kernels for the effects with settings.
void		BlurKernel(double kernel[9]);
void		SharpenKernel(int intensity, double kernel[9]);

// Fills the 4-connected area of pixels equal to the one at (x, y) with
// color. Returns the number of pixels changed.
int64_t		FloodFill(const Pixels& image, int32_t x, int32_t y,
				uint32_t color);

// Nearest-neighbour scaling of all of src into all of dst.
void		Scale(const Pixels& src, const Pixels& dst);

// dst must be src.height wide and src.width high.
void		Rotate90(const Pixels& src, const Pixels& dst, bool clockwise);

void		FillRect(const Pixels& image, int32_t x, int32_t y,
				int32_t width, int32_t height, uint32_t color);

// Copies a width x height block, clipped to both images.
void		Blit(const Pixels& src, int32_t srcX, int32_t srcY,
				int32_t width, int32_t height, const Pixels& dst,
				int32_t dstX, int32_t dstY);

// The spray tool's twelve dots around the pointer, in three rings, as in
// the original. random() must return a value in [0, bound).
struct SprayDot {
	int32_t	x;
	int32_t	y;
};
void		SprayPattern(double penSize, int (*random)(int bound),
				SprayDot dots[12]);

}	// namespace ImageOps

#endif	// IMAGE_OPS_H
