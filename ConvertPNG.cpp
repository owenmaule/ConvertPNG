#include "png++-0.2.9/png.hpp"
#include "png++-0.2.9/image.hpp"
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
#define LONG_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN
#define LONG_TO_BINARY(long) BYTE_TO_BINARY(long >> 24), BYTE_TO_BINARY(long >> 16), BYTE_TO_BINARY(long >> 8), BYTE_TO_BINARY(long)

// Trim off a few rows at the bottom of each cell
// Idea: Make such things user-facing configurable features i.e. command line parameters
uint_32 trim_bottom = 0;

// E.g. We don't need to store a bitmap of a whitespace
// Idea: Make such things user-facing configurable features
uint_32 skipCell [] = {0};
uint_32 skip = 0;

uint_32 debug_limit = 0;

int main(int argc, char* argv[])
{
	image<rgb_pixel> sourceImage = image<rgb_pixel>();
	uint_32 image_width = 0,
		image_height = 0;

	if (argc > 2) {
		// To do: Check the file to avoid crashing on invalid files
		sourceImage.read(argv[1]);
		image_width = sourceImage.get_width();
		image_height = sourceImage.get_height();
		printf("Image size: %d * %d\n", image_width, image_height);
	}

	if (argc < 4) {
		printf("Usage: %s <png file> <symbol width> <symbol height> (<symbols>)\n", argv[0]);
		return 1;
	}

	uint_32 symbol_width = atoi(argv[2]),
		symbol_height = atoi(argv[3]),
		symbols_per_row = image_width / symbol_width,
		total_symbols = symbols_per_row * (image_height / symbol_height),
		symbols = total_symbols;

	// I'll make it more general next time. Feel free to submit a pull request, thanks!
	if (symbol_width % 32 != 0) {
		printf("Restricted to multiples of 32 width for now\n");
		return 32;
	}

	// Following is more of a sanity check and might not always be useful. You can adjust the image in an image editor.
	// Nice free editor > https://www.gimp.org/
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

		if (limit_symbols < total_symbols) {
			symbols = limit_symbols;
			printf("Limiting symbols to %d\n", symbols);
		}
	}

	if (skip) {
		// validate skipCell[]
		for (uint_32 find = 0; find != skip; ++find)
			if (skipCell[find] >= symbols) {
				printf("Invalid skip cell entry %d for symbols %d\n", skipCell[find], symbols);
				return 4;
			}

		printf("Be aware, we are skipping %d cells\n", skip);
	}

	if (trim_bottom) {
		if (trim_bottom >= symbol_height) {
			printf("Bottom trim is %d equal or greater to cell height %d\n", trim_bottom, symbol_height);
			return 4;
		}

		printf("Be aware, we are trimming %d pixels from the bottom of cells\n", trim_bottom);
	}

	// Currently set up to for 3 bpp, so you have to customise this otherwise
	// Idea: Make the tool more flexible and user facing, with more parameters
#define MAX_PALETTE 8
	const uint_32 bits_per_pixel = 3;
	const uint_32 symbol_length = bits_per_pixel * (symbol_width / 32) * (symbol_height - trim_bottom);
	const uint_32 value_count = symbol_length * (symbols - skip);

	printf("Symbol width: %d Symbol height: %d Symbols: %d\n", symbol_width, symbol_height, symbols);
	printf("Symbols per row: %d Symbol length: %d TotalValue: %d Values: %d\n", symbols_per_row, symbol_length, symbol_length * (total_symbols - skip), value_count);
	 
	// Palette
	// This could be done inline but i felt for the current usage, it would be simpler with a pre-pass
	uint_32 palette_values = 0;
	// Extra slot because I'm going to let it go 1 over to catch the situation that more were required
	rgb_pixel palette[MAX_PALETTE + 1];

	for (uint_32 y = 0, y_end = image_height; y != y_end; ++y) {
		for (uint_32 x = 0, x_end = image_width; x != x_end; ++x) {
			rgb_pixel rgb = sourceImage.get_pixel(x, y);
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

	if (debug_limit)
		printf("Warning: debug_limit = %d\n", debug_limit);

	printf("-------------------->8--- CODE BELOW --->8------------------------\n");

	// Idea: Could throughly generate the code to be produced
//	printf("\n#define VALUES %d\n#define SYMBOL_WIDTH %d\n"#);

	printf("vec3 palette[%d] = vec3[](\n", palette_values);
	for (uint_32 i = 0; i != palette_values; ++i) {
		rgb_pixel rgb = palette[i];
		if (rgb.red == rgb.green && rgb.green == rgb.blue)
			printf("\tvec3(%.2g)%c ", rgb.red / 256., i != palette_values - 1 ? ',' : '\n');
		else
			printf("\tvec3(%.2g, %.2g, %.2g)%c ", rgb.red / 256., rgb.green / 256., rgb.blue / 256., i != palette_values - 1 ? ',' : '\n');
	}
	printf("\t);\n\n");

	printf("#define BM_LEN %d\n", value_count);
	printf("uint bitmap[BM_LEN] = uint[](\n\t");

	uint_32 value_index = 0;

	// A grid of regularly sized symbols
	// To do: clip an arbitary rect from a bitmap, sizes not multiples of 32
	for (uint_32 symbol = 0; symbol != symbols; ++symbol) {
		uint_32 x_start = (symbol % symbols_per_row) * symbol_width,
			x_end = x_start + symbol_width,
			y_start = symbol / symbols_per_row * symbol_height,
			y_end = y_start + symbol_height - trim_bottom;

		for (uint_32 find = 0; find != skip; ++find)
			if (skipCell[find] == symbol)
				goto skip;

		DEBUG_PRINTF("Symbol %d / %d X: %d-%d Y: %d-%d\n", symbol, symbols, x_start, x_end, y_start, y_end);
		for (uint_32 y = y_start; y != y_end; ++y) {

			// Wider bitmaps, multiple of 32 wide
			for (uint_32 chunk = x_start; chunk != x_end; chunk += 32) {

				// Bits per pixel separated into different rows- interleaved bitplanes
				for (uint_32 plane = 0; plane != bits_per_pixel; ++plane) {
					DEBUG_PRINTF("\nY: %d Chunk: %d Plane: %d\n", y, chunk, plane);

					// This will be our finished data row
					uint_32 value = 0;
					// The bit we're looking for
					uint_32 plane_mask = 1u << plane;
					DEBUG_PRINTF("Reset accummulator\n");

					for (uint_32 x = chunk; x != chunk + 32; ++x) {
						rgb_pixel rgb = sourceImage.get_pixel(x, y);
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
						DEBUG_PRINTF(LONG_TO_BINARY_PATTERN "   SHIFT->   ", LONG_TO_BINARY(value));
						// Shift 0 into 0 the first time, because we don't want to shift it again after finishing
						value = value >> 1;
						DEBUG_PRINTF(LONG_TO_BINARY_PATTERN "\n", LONG_TO_BINARY(value));
						// If the bit to match our palette index is present, then whack a 1 on the far end... with subsequent shifts it will end up in the right place
						if (i & plane_mask)
							value += (1u << 31);
						DEBUG_PRINTF(LONG_TO_BINARY_PATTERN "   ", LONG_TO_BINARY(value));
						DEBUG_PRINTF("(%d, %d) Palette index %d. Plane %d = %d. Accummulated %u Value index %d\n", x, y, i, plane_mask, i & plane_mask, value, value_index);
					}

//					printf("0x%00Xu, ", value); // hex, it's longer!
					printf("%uu%c ", value, ++value_index == value_count ? ' ' : ','); // decimal is shorter!
				}
				if (!--debug_limit) {
					printf("\n\nAborting for debug purposes (set debug_limit=0 to turn off)\n");
					return 5;
				}
			}
		}
skip:
		printf("\n\t");
	}
	printf(");\n");
	printf("Wrote %d / %d\n", value_index, value_count);

	return 0;
}

