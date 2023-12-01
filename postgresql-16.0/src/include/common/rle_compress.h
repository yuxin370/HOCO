/* ----------
 * rle_compress.h -
 *
 *	Definitions for the builtin RLE compressor
 *
 *  src/include/common/rle_compress.h
 * ----------
 */

#ifndef _RLE_COMPRESS_H_
#define _RLE_COMPRESS_H_

typedef struct RLE_Strategy
{
    int32 min_input_size;
    int32 max_input_size;
    int32 min_comp_rate;
}RLE_Strategy;
extern PGDLLIMPORT const RLE_Strategy *const RLE_strategy_default;
extern int32 rle_compress(const char *source, int32 slen, char *dest,const RLE_Strategy *strategy);
extern int32 rle_decompress(const char *source, int32 slen, char *dest,
				int32 rawsize, bool check_complete);

#endif