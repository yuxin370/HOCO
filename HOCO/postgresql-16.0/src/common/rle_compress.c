/**
 * 2023.9.30
 * Yuxin Tang
 * 
 * rle_compress.c
 * 
 *    an implementation of Run Length Encoding for PostgreSQL
 * 
 * 
 * 
 *    Compression
 *    When it comes to a single piece of non-repeating data, 
 *    the number of discontinuous data in this interval is counted, 
 *    and then a marker is used to indicate how many of the next 
 *    pieces of data are discontinuous. Because of the need to 
 *    distinguish between this mark byte is marked repeated data or not
 *    repeated data, so take the highest bit of the byte to do the mark, 
 *    for 0 indicates that it is not repeated data, for 1 indicates that 
 *    it is repeated data, the remaining 7 bits are used to indicate the 
 *    number of data, so the maximum can mark 127 bytes.
 *    Because when there are only 2 duplicate data, the duplicate data storage  
 *    format will still take up two bytes, so use the non-duplicate data sorage 
 *    format to store. Therefore,  the duplicate data mark can not be 0-2,  we 
 *    can use 0 to indicate 3, and the maximum number of duplicate bytes can be 
 *    expanded to 130.
 * 
*/

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif


#include <limits.h>
#include <string.h>
#include "common/rle_compress.h"

/* ----------
 * Local definitions
 * ----------
 */
#define MAX_REPEATED_SIZE 130
#define MAX_SINGLE_STORE_SIZE 127
#define THRESHOLD 3

#define buf_copy_ctrl(__buf,__dp) \
do { \
    *__buf = *__dp;                     \
    __buf ++;                           \
    __dp ++;                            \
}while(0)

#define store_single_buf(__bp,__buf_base,__buf,reach_max) \
do{ \
    if(reach_max){                                                  \
        *__bp = MAX_SINGLE_STORE_SIZE ;                             \
        __bp++;                                                     \
        memcpy(__bp,__buf_base,MAX_SINGLE_STORE_SIZE);              \
        __bp += MAX_SINGLE_STORE_SIZE;                              \
        memcpy(__buf_base,                                          \
                __buf_base + MAX_SINGLE_STORE_SIZE,                 \
                __buf - __buf_base - MAX_SINGLE_STORE_SIZE);        \
        __buf = __buf_base;                                         \
    }else{                                                          \
        *__bp = __buf - __buf_base ;                                \
        __bp++;                                                     \
        memcpy(__bp,__buf_base,__buf-__buf_base);                   \
        __bp += __buf - __buf_base;                                 \
        __buf = __buf_base;                                         \
    }                                                               \
}while(0)

/**
 * provided standard strategies
 * basic compression restriction
*/
static const RLE_Strategy rle_default_strategy = {
	32,							/* Data chunks less than 32 bytes are not
								 * compressed */
	INT_MAX,					/* No upper limit on what we'll try to
								 * compress */
	25							/* Require 25% compression rate, or not worth
								 * it */
};
const RLE_Strategy *const RLE_strategy_default = &rle_default_strategy;


/**
 * rle_compress -
 * 
 *            Compress Source into dest. 
 *            Return the number of bytes written in buffer dest, 
 *            or -1 if compression fails.
*/
int32 rle_compress(const char *source, int32 slen, char *dest,
                    const RLE_Strategy *strategy){
    
	const char *dp = source;                            //uncompressed data
	const char *dend = source + slen;                   //end of uncompressed data
	unsigned char *bp = (unsigned char *) dest;         //compressed data
	unsigned char *bstart = bp;                         //start of compressed data
    int32 need_rate;
    int32 result_max;
    int32 result_size;
    int32 cur_index = 0;
    char cur_char;
    char *buf = (char *)palloc(MAX_SINGLE_STORE_SIZE);
    char *buf_base;
    buf_base = buf;

    /**
     * Our fallback strategy is default.
    */
    if (strategy == NULL)
        strategy = RLE_strategy_default;

    if(slen < strategy->min_input_size || 
        slen > strategy->max_input_size)
    {
        pfree(buf);
        return -1;
    }
        

    need_rate = strategy->min_comp_rate;
    if(need_rate < 0 ) need_rate = 0;
    else if(need_rate > 99) need_rate = 99;

	/*
	 * Compute the maximum result size allowed by the strategy, namely the
	 * input size minus the minimum wanted compression rate.  This had better
	 * be <= slen, else we might overrun the provided output buffer.
	 */
	if (slen > (INT_MAX / 100))
	{
		/* Approximate to avoid overflow */
		result_max = (slen / 100) * (100 - need_rate);
	}
	else
		result_max = (slen * (100 - need_rate)) / 100;
    

    while (THRESHOLD <= dend - dp){
        /**
         * if we already exceed the maximum result size, fail.
        */
        if(bp - bstart >= result_max) return -1;
        cur_index = 0;
        if((*(dp + cur_index)) == (*(dp+cur_index+1))){
            if((*(dp+cur_index+1)) == (*(dp+cur_index+2))){
                cur_index ++;
                while(dp + cur_index + 2 < dend  && (*(dp + cur_index + 1) == *(dp + cur_index + 2))){
                    cur_index ++;
                }
                if(buf!=buf_base){
                    store_single_buf(bp,buf_base,buf,false);
                }
                cur_char = *dp;
                while (cur_index > 0)
                {
                    *bp = ((cur_index + 2) > MAX_REPEATED_SIZE ? (MAX_REPEATED_SIZE-THRESHOLD) : (cur_index + 2 - THRESHOLD) ) | (1 << 7) ;
                    bp ++;
                    *bp = cur_char;
                    bp++;
                    dp += (cur_index + 2) > MAX_REPEATED_SIZE ? MAX_REPEATED_SIZE : (cur_index + 2);
                    cur_index -= MAX_REPEATED_SIZE;
                }
            }else{
                buf_copy_ctrl(buf,dp);
                buf_copy_ctrl(buf,dp);
            }


        }else{
            buf_copy_ctrl(buf,dp);
        }

        if(buf - buf_base >= MAX_SINGLE_STORE_SIZE){
            store_single_buf(bp,buf_base,buf,true);
        }        
    }

    memcpy(buf,dp,dend-dp);
    buf += (dend - dp);

    if(buf - buf_base >= MAX_SINGLE_STORE_SIZE){
        store_single_buf(bp,buf_base,buf,true);
    }  

    if(buf!=buf_base){
        store_single_buf(bp,buf_base,buf,false);
    }
    *bp = '\0';

    pfree(buf);
    result_size = bp - bstart;
    if(result_size >= result_max) return -1;
    return result_size;
}

/**
 * rle_decompress -
 * 
 * 
 *      Decompress source into dest.
 *      Return the number of bytes decompressed into the destination buffer, 
 *      or -1 if the compressed data is corrupted
 *  	
 *      If check_complete is true, the data is considered corrupted
 *		if we don't exactly fill the destination buffer.  Callers that
 *		are extracting a slice typically can't apply this check.
*/

int32
rle_decompress(const char *source, int32 slen, char *dest,
				int32 rawsize, bool check_complete){
	const unsigned char *sp;
	const unsigned char *srcend;
	unsigned char *dp;
	unsigned char *destend;
    int32 repeat_count;
    int32 single_count;
    char cur_data;

    sp = (const unsigned char *) source;
	srcend = ((const unsigned char *) source) + slen;
	dp = (unsigned char *) dest;
	destend = dp + rawsize;

	while (sp < srcend && dp < destend)
	{
        if((*sp) >= (1<<7)){ 
            repeat_count = (int32)((*sp) & 0x7F)+THRESHOLD;
            sp++;
            cur_data = *sp;
            for(int i = 0 ; i < repeat_count;i++){
                *dp = cur_data;
                dp++;
            }
            sp ++;
        }else{
            single_count = (*sp);
            sp++;

            memcpy(dp,sp,single_count);
            dp += single_count;
            sp += single_count;
        }
    }
    *dp = '\0';

	/*
	 * If requested, check we decompressed the right amount.
	 */
	if (check_complete && (dp != destend || sp != srcend))
		return -1;

	/*
	 * That's it.
	 */
	return (char *) dp - dest;

}