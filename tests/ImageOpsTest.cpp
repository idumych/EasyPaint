/*
 * EasyPaint for Haiku - unit tests for the portable pixel code.
 * Distributed under the terms of the MIT License.
 *
 * Build and run with "make test" on any host.
 */
#include "ImageOps.h"

#include <stdio.h>
#include <stdlib.h>

#include <vector>

using namespace ImageOps;


static int sFailures = 0;

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, \
				#condition); \
			sFailures++; \
		} \
	} while (0)


struct Image {
	std::vector<uint32_t>	data;
	Pixels					pixels;

	Image(int32_t width, int32_t height, uint32_t fill = 0xffffffff)
		:
		data(width * height, fill)
	{
		pixels.bits = data.data();
		pixels.width = width;
		pixels.height = height;
		pixels.stride = width;
	}
};


static void
TestPixelHelpers()
{
	uint32_t p = MakePixel(1, 2, 3, 4);
	CHECK(p == 0x04010203);
	CHECK(Red(p) == 1 && Green(p) == 2 && Blue(p) == 3 && Alpha(p) == 4);
}


static void
TestEffects()
{
	Image image(2, 1);
	image.pixels.At(0, 0) = MakePixel(255, 0, 0, 128);
	image.pixels.At(1, 0) = MakePixel(10, 20, 30);

	Negative(image.pixels);
	CHECK(image.pixels.At(0, 0) == MakePixel(0, 255, 255, 128));
	CHECK(image.pixels.At(1, 0) == MakePixel(245, 235, 225));

	Image gray(1, 1);
	gray.pixels.At(0, 0) = MakePixel(255, 0, 0);
	Gray(gray.pixels);
	CHECK(gray.pixels.At(0, 0) == MakePixel(76, 76, 76));

	Image bin(2, 1);
	bin.pixels.At(0, 0) = MakePixel(200, 200, 200);
	bin.pixels.At(1, 0) = MakePixel(20, 20, 20, 7);
	Binarize(bin.pixels, 128);
	CHECK(bin.pixels.At(0, 0) == 0xffffffff);
	CHECK(bin.pixels.At(1, 0) == MakePixel(0, 0, 0, 7));

	Image gamma(1, 1);
	gamma.pixels.At(0, 0) = MakePixel(0, 128, 255);
	Gamma(gamma.pixels, 2);
	CHECK(gamma.pixels.At(0, 0) == MakePixel(0, 64, 255));
}


static void
TestConvolve()
{
	// A single black pixel on white, blurred, spreads to its neighbours;
	// the 2-pixel border is copied unchanged.
	Image src(7, 7);
	src.pixels.At(3, 3) = MakePixel(0, 0, 0);
	Image dst(7, 7, 0);
	double kernel[9];
	BlurKernel(kernel);
	Convolve(src.pixels, dst.pixels, kernel);
	CHECK(dst.pixels.At(0, 0) == 0xffffffff);
	CHECK(Red(dst.pixels.At(3, 3)) == 191);	// 255 * 12 / 16
	CHECK(Red(dst.pixels.At(2, 3)) == 223);	// 255 * 14 / 16
	CHECK(Red(dst.pixels.At(2, 2)) == 239);	// 255 * 15 / 16

	// The identity sharpen kernel (intensity 0) leaves the image as it is.
	SharpenKernel(0, kernel);
	Convolve(src.pixels, dst.pixels, kernel);
	CHECK(dst.pixels.At(3, 3) == MakePixel(0, 0, 0));
	CHECK(dst.pixels.At(2, 2) == 0xffffffff);
}


static void
TestFloodFill()
{
	// A white 5x5 image split by a black vertical line at x = 2.
	Image image(5, 5);
	for (int32_t y = 0; y < 5; y++)
		image.pixels.At(2, y) = MakePixel(0, 0, 0);

	int64_t count = FloodFill(image.pixels, 0, 0, MakePixel(255, 0, 0));
	CHECK(count == 10);
	CHECK(image.pixels.At(1, 4) == MakePixel(255, 0, 0));
	CHECK(image.pixels.At(3, 0) == 0xffffffff);
	CHECK(image.pixels.At(2, 2) == MakePixel(0, 0, 0));

	// Filling with the same colour, or outside the image, does nothing.
	CHECK(FloodFill(image.pixels, 0, 0, MakePixel(255, 0, 0)) == 0);
	CHECK(FloodFill(image.pixels, 9, 9, 0) == 0);

	// A large open area doesn't recurse.
	Image big(2000, 2000);
	CHECK(FloodFill(big.pixels, 1000, 1000, 0) == 2000 * 2000);
}


static void
TestGeometry()
{
	Image src(3, 2);
	for (int32_t y = 0; y < 2; y++) {
		for (int32_t x = 0; x < 3; x++)
			src.pixels.At(x, y) = y * 3 + x;
	}

	Image cw(2, 3);
	Rotate90(src.pixels, cw.pixels, true);
	// 0 1 2      3 0
	// 3 4 5  ->  4 1
	//            5 2
	CHECK(cw.pixels.At(0, 0) == 3 && cw.pixels.At(1, 0) == 0);
	CHECK(cw.pixels.At(0, 2) == 5 && cw.pixels.At(1, 2) == 2);

	Image ccw(2, 3);
	Rotate90(src.pixels, ccw.pixels, false);
	// 2 5
	// 1 4
	// 0 3
	CHECK(ccw.pixels.At(0, 0) == 2 && ccw.pixels.At(1, 0) == 5);
	CHECK(ccw.pixels.At(0, 2) == 0 && ccw.pixels.At(1, 2) == 3);

	Image twice(6, 4);
	Scale(src.pixels, twice.pixels);
	CHECK(twice.pixels.At(0, 0) == 0 && twice.pixels.At(1, 1) == 0);
	CHECK(twice.pixels.At(5, 3) == 5 && twice.pixels.At(2, 0) == 1);

	Image canvas(4, 4, 7);
	Blit(src.pixels, 0, 0, 3, 2, canvas.pixels, 2, 3);
	CHECK(canvas.pixels.At(2, 3) == 0 && canvas.pixels.At(3, 3) == 1);
	CHECK(canvas.pixels.At(1, 3) == 7 && canvas.pixels.At(2, 2) == 7);
	Blit(src.pixels, 0, 0, 3, 2, canvas.pixels, -2, -1);
	CHECK(canvas.pixels.At(0, 0) == 5);

	FillRect(canvas.pixels, -1, -1, 2, 2, 9);
	CHECK(canvas.pixels.At(0, 0) == 9 && canvas.pixels.At(1, 1) == 7);
}


static int
FixedRandom(int bound)
{
	return bound - 1;
}


static void
TestSpray()
{
	SprayDot dots[12];
	SprayPattern(4, FixedRandom, dots);
	CHECK(dots[0].x == 4 && dots[0].y == 4);	// (4 - 2) * 2
	CHECK(dots[4].x == 10);						// (9 - 4) * 2
	CHECK(dots[11].y == 14);					// (14 - 7) * 2
}


int
main()
{
	TestPixelHelpers();
	TestEffects();
	TestConvolve();
	TestFloodFill();
	TestGeometry();
	TestSpray();

	if (sFailures > 0) {
		fprintf(stderr, "%d check(s) failed\n", sFailures);
		return 1;
	}
	printf("all ImageOps tests passed\n");
	return 0;
}
