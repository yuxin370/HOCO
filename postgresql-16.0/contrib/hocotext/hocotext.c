/*
 * contrib/hocotext/hocotext.c
 */
#include "postgres.h"

#include "catalog/pg_collation.h"
#include "common/hashfn.h"
#include "utils/builtins.h"
#include "utils/formatting.h"

#include "access/table.h"
#include "access/tableam.h"
#include "utils/varlena.h"
#include "varatt.h"
#include "fmgr.h"
#include "access/detoast.h"

PG_MODULE_MAGIC;

/**
 * basic string operators
 * 1. extract
 * 2. insert
 * 3. delete
 * 4. symbol_compare
*/


/**
 * utility functions
*/


/*
 * hocotext_*_cmp()
 * Internal comparison function for hocotext strings (and common text strings).
 * Returns int32 negative, zero, or positive.
 */

struct varlena *hocotext_fetch_toasted_attr(struct varlena *attr);
static struct varlena *toast_fetch_datum(struct varlena *attr);

static int32 hocotext_hoco_cmp(struct varlena * left, struct varlena * right, Oid collid);
static int32 hocotext_mixed_cmp(struct varlena * left, struct varlena * right, Oid collid);
static int32 hocotext_common_cmp(struct varlena * left, struct varlena * right, Oid collid);


text * hocotext_hoco_extract(struct varlena * source,int32 offset,int32 len);
text * hocotext_hoco_insert(struct varlena * source,int32 offset,text *str);
text * hocotext_hoco_delete(struct varlena * source,int32 offset,int32 len);



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

PG_FUNCTION_INFO_V1(hocotext_extract);
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
 * *      UTILITY FUNCTIONS            *
 * *************************************
*/

struct varlena *
hocotext_fetch_toasted_attr(struct varlena *attr)
{
	if (VARATT_IS_EXTERNAL_ONDISK(attr))
	{
		/*
		 * This is an externally stored datum --- fetch it back from there
		 */
		attr = toast_fetch_datum(attr);
		/* If it's compressed, do nothing */

	}
	else if (VARATT_IS_EXTERNAL_INDIRECT(attr))
	{
		/*
		 * This is an indirect pointer --- dereference it
		 */
		struct varatt_indirect redirect;

		VARATT_EXTERNAL_GET_POINTER(redirect, attr);
		attr = (struct varlena *) redirect.pointer;

		/* nested indirect Datums aren't allowed */
		Assert(!VARATT_IS_EXTERNAL_INDIRECT(attr));

		/* recurse in case value is still extended in some other way */
		attr = hocotext_fetch_toasted_attr(attr);

		/* if it isn't, we'd better copy it */
		if (attr == (struct varlena *) redirect.pointer)
		{
			struct varlena *result;

			result = (struct varlena *) palloc(VARSIZE_ANY(attr));
			memcpy(result, attr, VARSIZE_ANY(attr));
			attr = result;
		}
	}
	else if (VARATT_IS_EXTERNAL_EXPANDED(attr))
	{
		/*
		 * This is an expanded-object pointer --- get flat format
		 */
		attr = detoast_external_attr(attr);
		/* flatteners are not allowed to produce compressed/short output */
		Assert(!VARATT_IS_EXTENDED(attr));
	}
	else if (VARATT_IS_COMPRESSED(attr))
	{
		/*
		 * This is a compressed value inside of the main tuple
		 */
		// attr = toast_decompress_datum(attr);
        // do nothing
	}
	else if (VARATT_IS_SHORT(attr))
	{
		/*
		 * This is a short-header varlena --- convert to 4-byte header format
		 */
		Size		data_size = VARSIZE_SHORT(attr) - VARHDRSZ_SHORT;
		Size		new_size = data_size + VARHDRSZ;
		struct varlena *new_attr;

		new_attr = (struct varlena *) palloc(new_size);
		SET_VARSIZE(new_attr, new_size);
		memcpy(VARDATA(new_attr), VARDATA_SHORT(attr), data_size);
		attr = new_attr;
	}

	return attr;
}

static struct varlena *
toast_fetch_datum(struct varlena *attr)
{
	Relation	toastrel;
	struct varlena *result;
	struct varatt_external toast_pointer;
	int32		attrsize;

	if (!VARATT_IS_EXTERNAL_ONDISK(attr))
		elog(ERROR, "toast_fetch_datum shouldn't be called for non-ondisk datums");

	/* Must copy to access aligned fields */
	VARATT_EXTERNAL_GET_POINTER(toast_pointer, attr);

	attrsize = VARATT_EXTERNAL_GET_EXTSIZE(toast_pointer);

	result = (struct varlena *) palloc(attrsize + VARHDRSZ);

	if (VARATT_EXTERNAL_IS_COMPRESSED(toast_pointer))
		SET_VARSIZE_COMPRESSED(result, attrsize + VARHDRSZ);
	else
		SET_VARSIZE(result, attrsize + VARHDRSZ);

	if (attrsize == 0)
		return result;			/* Probably shouldn't happen, but just in
								 * case. */

	/*
	 * Open the toast relation and its indexes
	 */
	toastrel = table_open(toast_pointer.va_toastrelid, AccessShareLock);

	/* Fetch all chunks */
	table_relation_fetch_toast_slice(toastrel, toast_pointer.va_valueid,
									 attrsize, 0, attrsize, result);

	/* Close toast table */
	table_close(toastrel, AccessShareLock);

	return result;
}

static int32 
hocotext_hoco_cmp(struct varlena * left, 
                    struct varlena * right, 
                    Oid collid)
{
    /**
     * TO DO
     * comparison between compressed attrs
    */
    return 1;
}

static int32 
hocotext_mixed_cmp(struct varlena * left, 
                    struct varlena * right, 
                    Oid collid)
{
    /**
     * TO DO
     * comparison between compressed attr and uncompressed attr
    */
    return 1;
}

static int32 
hocotext_common_cmp(struct varlena * left, 
                    struct varlena * right, 
                    Oid collid)
{
    /**
     * TO DO
     * comparison between uncompressed attrs
    */
    return 1;
}

text * 
hocotext_hoco_extract(struct varlena * source,
                        int32 offset,
                        int32 len){
    text *result;
    return result;
}

text * 
hocotext_hoco_insert(struct varlena * source,
                        int32 offset,
                        text *str){
    text *result;
    return result;
}
text * 
hocotext_hoco_delete(struct varlena * source,
                        int32 offset,
                        int32 len){
    text *result;
    return result;
}

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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) == 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) == 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) == 0);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) != 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) != 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) != 0);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) < 0);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) <= 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) <= 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) <= 0);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) > 0);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) >= 0);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) >= 0);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) >= 0);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    struct varlena *source_str = hocotext_fetch_toasted_attr(source);

    /**
     * TO DO
     * extract a substring of length len from the string source at position offset.
    */
    if(VARATT_IS_COMPRESSED(source)){
        result = hocotext_hoco_extract(source_str,offset,len);
    }else{
        /*result = user defined function for uncompressed text string*/
        // result = text_substring((*source_str),offset,len,0);
    }


   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}


Datum
hocotext_insert(PG_FUNCTION_ARGS){
    struct varlena *source = PG_GETARG_RAW_VARLENA_P(0);
    int32 offset = PG_GETARG_INT32(1);
    text *str = PG_GETARG_TEXT_P(2);
    text *result = NULL;
    struct varlena *source_str = hocotext_fetch_toasted_attr(source);

    /**
     * TO DO
     * locates offset_start, inserts new text str, and returns the evaluated compressed text
    */
    if(VARATT_IS_COMPRESSED(source)){
        result = hocotext_hoco_insert(source_str,offset,str);
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
    struct varlena *source_str = hocotext_fetch_toasted_attr(source);

    /**
     * TO DO
     * locates offset_start, deletes the sequence of given length len, and returns the evaluated compressed text
    */
    if(VARATT_IS_COMPRESSED(source)){
        result = hocotext_hoco_delete(source_str,offset,len);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0 ? left_datum : right_datum);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) < 0 ? left_datum : right_datum);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) < 0 ? left_datum : right_datum);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = (hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0 ? left_datum : right_datum);
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = (hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION()) > 0 ? left_datum : right_datum);

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = (hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION()) > 0 ? left_datum : right_datum);

    }
	pfree(left_datum);
	pfree(right_datum);
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
    bool is_comp_lef = VARATT_IS_COMPRESSED(left);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right);
    struct varlena *left_datum = hocotext_fetch_toasted_attr(left);
    struct varlena *right_datum = hocotext_fetch_toasted_attr(right);
   	if (is_comp_lef && is_comp_righ){
        /**
         * case 1: both left and right are compressed
         * compressed datum vs compressed datum
        */

        result = hocotext_hoco_cmp(left_datum,right_datum,PG_GET_COLLATION());
    }else if(is_comp_lef|| is_comp_righ){
        /**
        * case 2: only one arg is compressed
        * compressed datum vs uncompressed datum
        */

        result = hocotext_mixed_cmp(left_datum,right_datum,PG_GET_COLLATION());
        

    }else{
        /*
        * case 3 both left and right are uncompressed
        * uncompressed datum vs uncompressed datum
        */

        result = hocotext_common_cmp((text *)left_datum,(text *)right_datum,PG_GET_COLLATION());

    }
	pfree(left_datum);
	pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_INT32(result);
}


Datum
hocotext_hash(PG_FUNCTION_ARGS){

    struct varlena *str = PG_GETARG_RAW_VARLENA_P(0);

    struct varlena *str_datum = hocotext_fetch_toasted_attr(str);


    /**
     * do nothing
    */

    PG_RETURN_INT32(0);
}


