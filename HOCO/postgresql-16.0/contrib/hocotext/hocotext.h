/*
 * contrib/hocotext/hocotext.h
 */
#ifndef HOCOTEXT_HPP
#define HOCOTEXT_HPP

#include "postgres.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_compression_index.h"
#include "common/hashfn.h"
#include "utils/builtins.h"
#include "utils/formatting.h"

#include "access/table.h"
#include "access/tableam.h"
#include "utils/varlena.h"
#include "varatt.h"
#include "fmgr.h"
#include "access/detoast.h"
#include "access/toast_internals.h"
#include "postgres_ext.h"

/**
 * utility functions
*/

#define THRESHOLD 3
#define repeat_buf_copy(__dp,__str,__count) \
do{ \
    for(int i = 0; i < __count; i++){       \
        *__dp = __str;                      \
        __dp ++;                            \
    }\
}while(0)


/**
 * ************************************************************
 *                  HLEP HANDLE FUNCTIONS                     *
 * ************************************************************
*/
extern int32 hocotext_hoco_cmp_helper(struct varlena * left, struct varlena * right, Oid collid);
extern text * hocotext_hoco_extract_helper(struct varlena * source,int32 offset,int32 len,Oid collid);
extern text * hocotext_hoco_extract_with_index_optimization_helper(struct varlena * source,int32 offset,int32 len,Oid collid);
/**
 * ************************************************************
 *                 COMMON UTILITY FUNCTIONS                   *
 * ************************************************************
*/
extern int32 hocotext_common_cmp(struct varlena * left, struct varlena * right, Oid collid);
extern text * hocotext_common_extract(struct varlena * source,int32 offset,int32 len,Oid collid);

/**
 * ************************************************************
 *                    RLE UTILITY FUNCTIONS                   *
 * ************************************************************
*/

/*
 * hocotext_rle_*_cmp()
 * Internal comparison function for hocotext_rle strings (and common text strings).
 * Returns int32 negative, zero, or positive.
 */

extern int32 hocotext_rle_hoco_cmp(struct varlena * left, struct varlena * right, Oid collid);
extern int32 hocotext_rle_mixed_cmp(struct varlena * left, struct varlena * right, Oid collid);

/**
 * hocotext_rle_*_*()
 * internal operation functions
*/
extern text * hocotext_rle_hoco_extract(struct varlena * source,int32 offset,int32 len,Oid collid);
extern text * hocotext_rle_hoco_insert(struct varlena * source,int32 offset,text *str);
extern text * hocotext_rle_hoco_delete(struct varlena * source,int32 offset,int32 len);

extern text * hocotext_rle_hoco_extract_with_index_optimization(struct varlena * source,int32 offset,int32 len,Oid collid);

/**
 * ************************************************************
 *                    LZ4 UTILITY FUNCTIONS                   *
 * ************************************************************
*/
extern text * hocotext_lz4_hoco_extract(struct varlena * source,int32 offset,int32 len,Oid collid);

#endif          /*HOCOTEXT_HPP*/