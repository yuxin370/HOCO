/*
 * contrib/hocotext/hocotext.c
 */
#include "hocotext.h"

PG_MODULE_MAGIC;

/**
 * basic string operators
 * 1. extract
 * 2. insert
 * 3. delete
 * 4. symbol_compare
*/

/**
 * help hander functions
*/

int32 hocotext_hoco_cmp_helper(struct varlena * left, struct varlena * right, Oid collid){
	ToastCompressionId cmidl,cmidr;

	Assert(VARATT_IS_COMPRESSED(left) && VARATT_IS_COMPRESSED(right));

	/*
	 * Fetch the compression method id stored in the compression header and
	 * process the data using the appropriate homomorphic computation routine.
	 */

	cmidl = TOAST_COMPRESS_METHOD(left);
	cmidr = TOAST_COMPRESS_METHOD(right);
	Assert(cmidl == cmidr);
	switch (cmidl)
	{
		case TOAST_PGLZ_COMPRESSION_ID:
			return 0;
		case TOAST_LZ4_COMPRESSION_ID:
			return 0;
		case TOAST_RLE_COMPRESSION_ID:
			return hocotext_rle_hoco_cmp(left,right,collid);
		default:
			elog(ERROR, "invalid compression method id %d", cmidr);
			return 0;		/* keep compiler quiet */
	}
}

text * hocotext_hoco_extract_helper(struct varlena * source,int32 offset,int32 len,Oid collid){
	ToastCompressionId cmid;

	Assert(VARATT_IS_COMPRESSED(source));

	/*
	 * Fetch the compression method id stored in the compression header and
	 * process the data using the appropriate homomorphic computation routine.
	 */

	cmid = TOAST_COMPRESS_METHOD(source);
	switch (cmid)
	{
		case TOAST_PGLZ_COMPRESSION_ID:
			return NULL;
		case TOAST_LZ4_COMPRESSION_ID:
			return hocotext_lz4_hoco_extract(source,offset,len,collid);
		case TOAST_RLE_COMPRESSION_ID:
			return hocotext_rle_hoco_extract(source,offset,len,collid);
		default:
			elog(ERROR, "invalid compression method id %d", cmid);
			return 0;		/* keep compiler quiet */
	}
}

text * hocotext_hoco_extract_helper_naive(struct varlena * source,int32 offset,int32 len,Oid collid){
	ToastCompressionId cmid;

	Assert(VARATT_IS_COMPRESSED(source));

	/*
	 * Fetch the compression method id stored in the compression header and
	 * process the data using the appropriate homomorphic computation routine.
	 */

	cmid = TOAST_COMPRESS_METHOD(source);
	switch (cmid)
	{
		case TOAST_PGLZ_COMPRESSION_ID:
			return NULL;
		case TOAST_LZ4_COMPRESSION_ID:
			return hocotext_lz4_hoco_extract_naive(source,offset,len,collid);
		case TOAST_RLE_COMPRESSION_ID:
			return hocotext_rle_hoco_extract(source,offset,len,collid);
		default:
			elog(ERROR, "invalid compression method id %d", cmid);
			return 0;		/* keep compiler quiet */
	}
}

text * hocotext_hoco_extract_with_index_optimization_helper(struct varlena * source,int32 offset,int32 len,Oid collid){
	ToastCompressionId cmid;

	Assert(VARATT_IS_COMPRESSED(source));

	/*
	 * Fetch the compression method id stored in the compression header and
	 * process the data using the appropriate homomorphic computation routine.
	 */

	cmid = TOAST_COMPRESS_METHOD(source);
	switch (cmid)
	{
		case TOAST_PGLZ_COMPRESSION_ID:
			return NULL;
		case TOAST_LZ4_COMPRESSION_ID:
			return NULL;
		case TOAST_RLE_COMPRESSION_ID:
			return hocotext_rle_hoco_extract_with_index_optimization(source,offset,len,collid);
		default:
			elog(ERROR, "invalid compression method id %d", cmid);
			return 0;		/* keep compiler quiet */
	}
}

/**
 * utility functions
*/

text * 
hocotext_common_extract(struct varlena * source,
                            int32 offset,
                            int32 len,
                            Oid collid){
    text *result = (text *)palloc(len + VARHDRSZ+10); 
    char * sp = VARDATA_ANY(source);
    char * srcend = (char *)source + VARSIZE(source);
    int32 result_len;

	if (!OidIsValid(collid))
	{
		/*
		 * This typically means that the parser could not resolve a conflict
		 * of implicit collations, so report it that way.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which collation to use for %s function",
						"hocotext_rle_extract()"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}


    if(offset > VARSIZE_ANY_EXHDR(source)){
        return NULL;
    }
    sp += offset;
    result_len = ((srcend - sp)< len? (srcend - sp):len);
    memcpy(VARDATA_ANY(result),sp,result_len);
    *(VARDATA_ANY(result) + result_len) = '\0';
    SET_VARSIZE(result,result_len+VARHDRSZ);
    return result;
}

int32 
hocotext_common_cmp(struct varlena * left, struct varlena * right, Oid collid)
{
    char * lefp = VARDATA_ANY(left);
    char * lefend = (char *)left + VARSIZE_ANY(left);
    
    char * righp = VARDATA_ANY(right);
    char * righend = (char *)right + VARSIZE_ANY(right);


	if (!OidIsValid(collid))
	{
		/*
		 * This typically means that the parser could not resolve a conflict
		 * of implicit collations, so report it that way.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which collation to use for %s function",
						"hocotext_rle_cmp()"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}

    while(lefp < lefend && righp < righend){


        if((*lefp) == (*righp)){
            lefp ++;
            righp ++;
            continue;
        }
        return (*lefp) > (*righp) ? 1 :  ((*lefp) == (*righp) ? 0 : -1); 
    }

    if(lefp < lefend) return 1;
    else if(righp < righend) return -1;
    else return 0;

}

/**
* input/output routines
* using internal text input/output routines
*/

/**
* operating functions
*/
PG_FUNCTION_INFO_V1(hocotext_eq); // == 
PG_FUNCTION_INFO_V1(hocotext_nq); // !=
PG_FUNCTION_INFO_V1(hocotext_lt); // <
PG_FUNCTION_INFO_V1(hocotext_le); // <=
PG_FUNCTION_INFO_V1(hocotext_gt); // >
PG_FUNCTION_INFO_V1(hocotext_ge); // >=

PG_FUNCTION_INFO_V1(text_extract);
PG_FUNCTION_INFO_V1(hocotext_extract);
PG_FUNCTION_INFO_V1(hocotext_extract_naive);
PG_FUNCTION_INFO_V1(hocotext_extract_optimized);
PG_FUNCTION_INFO_V1(hocotext_insert);
PG_FUNCTION_INFO_V1(hocotext_delete);


/**
 * aggregate functions
*/

PG_FUNCTION_INFO_V1(hocotext_smaller); // return the smaller text
PG_FUNCTION_INFO_V1(hocotext_larger);  // return the larger text


/**
 * indexing support functions 
*/

PG_FUNCTION_INFO_V1(hocotext_cmp);
PG_FUNCTION_INFO_V1(hocotext_hash);
// PG_FUNCTION_INFO_V1(hocotext_hash_extended); // to be understood for its usage


/**
 * *************************************
 * *      INPUT/OUTPUT ROUTINES        *
 * *************************************
*/

/**
 * *************************************
 * *       OPERATING FUNCTIONS         *
 * *************************************
*/

Datum
hocotext_eq(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    bool result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) == 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) == 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp(left_datum,right_datum,PG_GET_COLLATION()) == 0);
    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_BOOL(result);
}

Datum
hocotext_nq(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    bool result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) != 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) != 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp(left_datum,right_datum,PG_GET_COLLATION()) != 0);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_BOOL(result);
}

Datum
hocotext_lt(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    bool result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) < 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_BOOL(result);
}

Datum
hocotext_le(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    bool result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) <= 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) <= 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp(left_datum,right_datum,PG_GET_COLLATION()) <= 0);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_BOOL(result);
}

Datum
hocotext_gt(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    bool result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) > 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_BOOL(result);
}

Datum
hocotext_ge(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    bool result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */

    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) >= 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) >= 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp(left_datum,right_datum,PG_GET_COLLATION()) >= 0);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_BOOL(result);
}

Datum
hocotext_extract(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_RAW_VARLENA_P(0);
    int32 offset = PG_GETARG_INT32(1);
    int32 len = PG_GETARG_INT32(2);
    text *result = NULL;
    struct varlena *source_str = (text *) pg_detoast_datum_packed_without_decompression(source);
    /**
     * TO DO
     * extract a substring of length len from the string source at position offset.
    */
    if(VARATT_IS_COMPRESSED(source_str)){
        result = hocotext_hoco_extract_helper(source_str,offset,len,PG_GET_COLLATION());
    }else{
        result = hocotext_common_extract(source_str,offset,len,PG_GET_COLLATION());
    }


   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}

Datum
hocotext_extract_naive(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_RAW_VARLENA_P(0);
    int32 offset = PG_GETARG_INT32(1);
    int32 len = PG_GETARG_INT32(2);
    text *result = NULL;
    struct varlena *source_str = (text *) pg_detoast_datum_packed_without_decompression(source);
    /**
     * TO DO
     * extract a substring of length len from the string source at position offset.
    */
    if(VARATT_IS_COMPRESSED(source_str)){
        result = hocotext_hoco_extract_helper_naive(source_str,offset,len,PG_GET_COLLATION());
    }else{
        result = hocotext_common_extract(source_str,offset,len,PG_GET_COLLATION());
    }


   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}


/**
 * TO DO
 * 1、首先确认LZ4压缩可以使用
 * 2、查看LZ4压缩的文本长什么样子
 * 3、确认LZ4的extract和cmp算法
 * 4、进行实现。
 * 
*/

Datum
hocotext_extract_optimized(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_RAW_VARLENA_P(0);
    int32 offset = PG_GETARG_INT32(1);
    int32 len = PG_GETARG_INT32(2);
    text *result = NULL;
    struct varlena *source_str = (text *) pg_detoast_datum_packed_without_decompression(source);
    /**
     * TO DO
     * extract a substring of length len from the string source at position offset.
    */
    if(VARATT_IS_COMPRESSED(source_str)){
        result = hocotext_hoco_extract_with_index_optimization_helper(source_str,offset,len,PG_GET_COLLATION());
    }else{
        result = hocotext_common_extract(source_str,offset,len,PG_GET_COLLATION());
    }


   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}

Datum
text_extract(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_TEXT_PP(0);
    int32 offset = PG_GETARG_INT32(1);
    int32 len = PG_GETARG_INT32(2);
    text *result = NULL;
    /**
     * TO DO
     * extract a substring of length len from the string source at position offset.
    */
    result = hocotext_common_extract(source,offset,len,PG_GET_COLLATION());


   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}


Datum
hocotext_insert(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_RAW_VARLENA_P(0);
    int32 offset = PG_GETARG_INT32(1);
    text *str = PG_GETARG_TEXT_P(2);
    text *result = NULL;
    struct varlena *source_str = (text *) pg_detoast_datum_packed_without_decompression(source);

    /**
     * TO DO
     * locates offset_start, inserts new text str, and returns the evaluated compressed text
    */
    if(VARATT_IS_COMPRESSED(source)){
        result = hocotext_rle_hoco_insert(source_str,offset,str);
    }else{
        /*result = user defined function for uncompressed text string*/
    }


   PG_FREE_IF_COPY(source,0);
   PG_FREE_IF_COPY(str,2);

   PG_RETURN_TEXT_P(result);
}

Datum
hocotext_delete(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_RAW_VARLENA_P(0);
    int32 offset = PG_GETARG_INT32(1);
    int32 len = PG_GETARG_INT32(2);
    text *result = NULL;
    struct varlena *source_str = (text *) pg_detoast_datum_packed_without_decompression(source);

    /**
     * TO DO
     * locates offset_start, deletes the sequence of given length len, and returns the evaluated compressed text
    */
    if(VARATT_IS_COMPRESSED(source)){
        result = hocotext_rle_hoco_delete(source_str,offset,len);
    }else{
        /*result = user defined function for uncompressed text string*/
    }

   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}




/**
 * *************************************
 * *      AGGREGATE FUNCTIONS          *
 * *************************************
*/

Datum
hocotext_smaller(PG_FUNCTION_ARGS){
    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    text *result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) < 0 ? left_datum : right_datum);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0 ? left_datum : right_datum);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) < 0 ? left_datum : right_datum);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_TEXT_P((text *)result);
}

Datum
hocotext_larger(PG_FUNCTION_ARGS){

    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    text *result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION()) > 0 ? left_datum : right_datum);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0 ? left_datum : right_datum);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) > 0 ? left_datum : right_datum);

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_TEXT_P((text *)result);
}




/**
 * *************************************
 * *      INDEXING FUNCTIONS          *
 * *************************************
*/

Datum
hocotext_cmp(PG_FUNCTION_ARGS){

    struct varlena *left = PG_GETARG_RAW_VARLENA_P(0);
    struct varlena *right = PG_GETARG_RAW_VARLENA_P(1);
    int32 result;

    /**
     * case 1: both left and right are compressed
     * case 2: only one arg is compressed
     * case 3: both left and right are uncompressed
    */
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = hocotext_hoco_cmp_helper(left_datum,right_datum,PG_GET_COLLATION());
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = hocotext_rle_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION());
        

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION());

    }
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_INT32(result);
}


Datum
hocotext_hash(PG_FUNCTION_ARGS){

    // struct varlena *str = PG_GETARG_RAW_VARLENA_P(0);

    // struct varlena *str_datum = (text *) pg_detoast_datum_packed_without_decompression(str);

    /**
     * do nothing
    */

    PG_RETURN_INT32(0);
}


