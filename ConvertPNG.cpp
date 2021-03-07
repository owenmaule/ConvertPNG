#include "png++-0.2.9/png.hpp"
using namespace png;

#define DEBUG_PRINTF // printf

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 


int main(int argc, char* argv[])
{
	if (argc < 4) {
		printf("Usage: %s <png file> <symbol width> <symbol height> (<symbols>)\n", argv[0]);
		return 1;
	}
	// To do: Check the file to avoid crashing on invalid files
	image<rgb_pixel> image(argv[1]);
	uint_32 image_width = image.get_width(),
		image_height = image.get_height();
	printf("Image size: %d * %d\n", image_width, image_height);

	uint_32 symbol_width = atoi(argv[2]),
		symbol_height = atoi(argv[3]),
		symbols_per_row = image_width / symbol_width,
		symbols = symbols_per_row * (image_height / symbol_height);

	// I'll make it more general next time. Feel free to submit a pull request, thanks!
	if (symbol_width != 32) {
		printf("Restricted to 32 symbol width for now\n");
		return 32;
	}

	// Following is more of a sanity check and might not always be useful. You can adjust the image in an image editor.
	if (image_width % symbol_width) {
		printf("Image width %d not divisible by symbol width %d\n", image_width, symbol_width);
		return 2;
	}
	if (image_height % symbol_height) {
		printf("Image height %d not divisible by symbol height %d\n", image_height, symbol_height);
		return 2;
	}

	if (argc > 4) {
		// Partial run for testing
		uint_32 limit_symbols = atoi(argv[4]);

		if (limit_symbols < symbols) {
			symbols = limit_symbols;
			printf("Limiting symbols to %d\n", symbols);
		}
	}

	printf("Symbol width: %d Symbol height: %d Symbols: %d Symbols per row: %d Symbol length: %d Values %d\n", symbol_width, symbol_height, symbols, symbols_per_row, symbol_width * symbol_height, symbol_width * symbol_height * symbols);

	// Idea: Could throughly generate the code to be produced
//	printf("\n#define VALUES %d\n#define SYMBOL_WIDTH %d\n"#);

	// Currently set up to for 3 bpp, so you have to customise this otherwise
	// Idea: Make the tool more flexible and user facing, with more parameters
#define MAX_PALETTE 8
	// Palette
	// This could be done inline but i felt for the current usage, it would be simpler with a pre-pass
	uint_32 palette_values = 0;
	// Extra slot because I'm going to let it go 1 over to catch the situation that more were required
	rgb_pixel palette[MAX_PALETTE + 1];

	for (uint_32 y = 0, y_end = image_height; y != y_end; ++y) {
		for (uint_32 x = 0, x_end = image_width; x != x_end; ++x) {
			rgb_pixel rgb = image.get_pixel(x, y);
			for (uint_32 i = 0; i != palette_values; ++i) {
				rgb_pixel compare = palette[i];
				if (compare.red == rgb.red && compare.green == rgb.green && compare.blue == rgb.blue)
					goto outer;
			}
			palette[palette_values] = rgb;
			if (++palette_values > MAX_PALETTE) {
				printf("Exceeded maximum palette of %d\n", MAX_PALETTE);
				return 3;
			}
		outer:;
		}
	}

	printf("Found %d palette values\n", palette_values);
	for (uint_32 i = 0; i != palette_values; ++i) {
		rgb_pixel rgb = palette[i];
		printf("vec3(%.2g, %.2g, %.2g), ", rgb.red / 256., rgb.green / 256., rgb.blue / 256.);
	}
	printf("\n\n");

	// Trim off a few rows at the bottom of each cell
	// Idea: Make such things user-facing configurable features
#define DEAD_BOTTOM 2

	// A grid of regularly sized symbols
	// To do: Make this an optional feature - we may want a whole bitmap, or clip just a rect from one
	for (uint_32 symbol = 0; symbol != symbols; ++symbol) {
		uint_32 x_start = (symbol % symbols_per_row) * symbol_width,
			x_end = x_start + symbol_width,
			y_start = symbol / symbols_per_row * symbol_height,
			y_end = y_start + symbol_height - DEAD_BOTTOM;

		// We don't need to store a bitmap of a whitespace
		// Idea: Make such things user-facing configurable features
		if (symbol == 29)
			continue;

		DEBUG_PRINTF("Symbol %d / %d X: %d-%d Y: %d-%d\n", symbol, symbols, x_start, x_end, y_start, y_end);
		for (uint_32 y = y_start; y != y_end; ++y) {

			// Bits per pixel separated into different rows- interleaved bitplanes
			for (uint_32 plane = 0; plane != 3; ++plane) {

				// This will be our finished data row
				uint_32 value = 0;
				// The bit we're looking for
				uint_32 plane_mask = 1u << plane;
				DEBUG_PRINTF("Reset cummulative\n");

				for (uint_32 x = x_start; x != x_end; ++x) {
					rgb_pixel rgb = image.get_pixel(x, y);
					uint_32 i = 0;
					// Get the palete entry
					for (; i != MAX_PALETTE; ++i) {
						rgb_pixel compare = palette[i];
						if (compare.red == rgb.red && compare.green == rgb.green && compare.blue == rgb.blue)
							break;
					}
					// It is surely there, we did a pre-pass
					assert(i != MAX_PALETTE);
					// The colours looked wrong so i was checking the bits (the problem was in GLSL code)
					DEBUG_PRINTF(BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN "   SHIFT->   ", BYTE_TO_BINARY(value >> 24), BYTE_TO_BINARY(value >> 16), BYTE_TO_BINARY(value >> 8), BYTE_TO_BINARY(value));
					// Shift 0 into 0 the first time, because we don't want to shift it again after finishing
					value = value >> 1;
					DEBUG_PRINTF(BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN "\n", BYTE_TO_BINARY(value >> 24), BYTE_TO_BINARY(value >> 16), BYTE_TO_BINARY(value >> 8), BYTE_TO_BINARY(value));
					// If the bit to match our palette index is present, then whack a 1 on the far end... with subsequent shifts it will end up in the right place
					if (i & plane_mask)
						value += (1u << 31);
					DEBUG_PRINTF(BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN "   ", BYTE_TO_BINARY(value >> 24), BYTE_TO_BINARY(value >> 16), BYTE_TO_BINARY(value >> 8), BYTE_TO_BINARY(value));
					DEBUG_PRINTF("(%d, %d) Palette index %d. Plane %d = %d. Cummulative %u\n", x, y, i, plane_mask, i & plane_mask, value);
				}

				if (value)
					printf("0x%00Xu, ", value);
				else
					printf("0u, ");
			}
		}
		printf("\n");
	}
	return 0;
}

