/**
 * minarch_zip.c - ZIP archive extraction utilities
 *
 * Provides functions to extract files from ZIP archives, supporting
 * both uncompressed (stored) and deflate-compressed files.
 *
 * Extracted from minarch.c for testability.
 */

#include "minarch_zip.h"
#include <stdint.h>
#include <zlib.h>

// Use smaller of two values
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

int MinArchZip_copy(FILE* zip, FILE* dst, size_t size) {
	uint8_t chunk[MINARCH_ZIP_CHUNK_SIZE];

	while (size) {
		size_t sz = MIN(size, MINARCH_ZIP_CHUNK_SIZE);
		if (sz != fread(chunk, 1, sz, zip))
			return -1;
		if (sz != fwrite(chunk, 1, sz, dst))
			return -1;
		size -= sz;
	}
	return 0;
}

int MinArchZip_inflate(FILE* zip, FILE* dst, size_t size) {
	z_stream stream = {0};
	size_t have = 0;
	uint8_t in[MINARCH_ZIP_CHUNK_SIZE];
	uint8_t out[MINARCH_ZIP_CHUNK_SIZE];
	int ret = -1;

	// Initialize zlib with raw deflate (no header)
	ret = inflateInit2(&stream, -MAX_WBITS);
	if (ret != Z_OK)
		return ret;

	do {
		size_t insize = MIN(size, MINARCH_ZIP_CHUNK_SIZE);

		stream.avail_in = fread(in, 1, insize, zip);
		if (ferror(zip)) {
			(void)inflateEnd(&stream);
			return Z_ERRNO;
		}

		if (!stream.avail_in)
			break;
		stream.next_in = in;

		do {
			stream.avail_out = MINARCH_ZIP_CHUNK_SIZE;
			stream.next_out = out;

			ret = inflate(&stream, Z_NO_FLUSH);
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
				/* fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				(void)inflateEnd(&stream);
				return ret;
			}

			have = MINARCH_ZIP_CHUNK_SIZE - stream.avail_out;
			if (fwrite(out, 1, have, dst) != have || ferror(dst)) {
				(void)inflateEnd(&stream);
				return Z_ERRNO;
			}
		} while (stream.avail_out == 0);

		size -= insize;
	} while (size && ret != Z_STREAM_END);

	(void)inflateEnd(&stream);

	if (!size || ret == Z_STREAM_END) {
		return Z_OK;
	} else {
		return Z_DATA_ERROR;
	}
}
