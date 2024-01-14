/*
 * contrib/hocotext/hocotext.c
 */
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
#define THRESHOLD 3
#define repeat_buf_copy(__dp,__str,__count) \
do{ \
    for(int i = 0; i < __count; i++){       \
        *__dp = __str;                      \
        __dp ++;                            \
    }\
}while(0)
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

static int32 hocotext_hoco_cmp(struct varlena * left, struct varlena * right, Oid collid);
static int32 hocotext_mixed_cmp(struct varlena * left, struct varlena * right, Oid collid);
static int32 hocotext_common_cmp(struct varlena * left, struct varlena * right, Oid collid);

/**
 * internal operation functions
*/
text * hocotext_hoco_extract(struct varlena * source,int32 offset,int32 len,Oid collid);
text * hocotext_hoco_insert(struct varlena * source,int32 offset,text *str);
text * hocotext_hoco_delete(struct varlena * source,int32 offset,int32 len);

text * hocotext_common_extract(struct varlena * source,int32 offset,int32 len,Oid collid);
text * hocotext_hoco_extract_with_index_optimization(struct varlena * source,int32 offset,int32 len,Oid collid);


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
 * *      UTILITY FUNCTIONS            *
 * *************************************
*/


static int32 
hocotext_hoco_cmp(struct varlena * left, 
                    struct varlena * right, 
                    Oid collid)
{

    char *lefp;
    char *lefend;
	char *righp;
    char *righend;
    char* cur_lef = (char *)palloc(131);
    char* cur_righ = (char *)palloc(131);
    char *cur_lef_p = cur_lef;
    char *cur_righ_p = cur_righ;
    int32 lef_count = 0;
    int32 righ_count = 0;

	if (!OidIsValid(collid))
	{
		/*
		 * This typically means that the parser could not resolve a conflict
		 * of implicit collations, so report it that way.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which collation to use for %s function",
						"hocotext_cmp()"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}

    lefp = (char *) left + VARHDRSZ_COMPRESSED + 8; // jump over 8 byte index info 
    righp = (char *) right + VARHDRSZ_COMPRESSED + 8;
    lefend = (char *) left + VARSIZE_ANY(left);
    righend = (char *) right + VARSIZE_ANY(right);


    while(lefp < lefend && righp < righend){
        if(lef_count == 0){
            lef_count =  (0x1&((*lefp) >> 7)) == 1 ? (int32)((*lefp) & 0x7f) + THRESHOLD :  (int32)((*lefp) & 0x7f);
            if((0x1&((*lefp) >> 7)) == 1) {
                lefp ++;
                cur_lef_p = cur_lef;
                repeat_buf_copy(cur_lef_p,(*(lefp)),lef_count);
                *cur_lef_p = '\0';
                lefp++;
            }else{
                lefp++;
                memcpy(cur_lef,(lefp),lef_count);
                *(cur_lef + lef_count) = '\0';
                lefp += lef_count;
            }
            cur_lef_p = cur_lef;
        }

        if(righ_count == 0){
            righ_count =  (0x1&((*righp) >> 7)) == 1 ? (int32)((*righp) & 0x7f) + THRESHOLD :  (int32)((*righp) & 0x7f);
            if((0x1&((*righp) >> 7)) == 1) {
                righp ++;
                cur_righ_p = cur_righ;
                repeat_buf_copy(cur_righ_p,(*(righp)),righ_count);
                *cur_righ_p = '\0';
                righp++;
            }else{
                righp++;
                memcpy(cur_righ,(righp),righ_count);
                *(cur_righ + righ_count) = '\0';
                righp += righ_count;
            }
            cur_righ_p = cur_righ;
        }
        

        while(lef_count >0 && righ_count > 0){

            if((*cur_lef_p) == (*cur_righ_p)){
                cur_lef_p ++;
                cur_righ_p++;
                lef_count --;
                righ_count --;
            }else{
                return (*cur_lef_p) > (*cur_righ_p) ? 1 :  ((*cur_lef_p) == (*cur_righ_p) ? 0 : -1);
            }
        }
    }
    
    while(lefp < lefend && righ_count > 0){
        if(lef_count == 0){
            lef_count =  (0x1&((*lefp) >> 7)) == 1 ? (int32)((*lefp) & 0x7f) + THRESHOLD :  (int32)((*lefp) & 0x7f);
            if((0x1&((*lefp) >> 7)) == 1) {
                lefp ++;
                cur_lef_p = cur_lef;
                repeat_buf_copy(cur_lef_p,(*(lefp)),lef_count);
                *cur_lef_p = '\0';
                lefp++;
            }else{
                lefp++;
                memcpy(cur_lef,(lefp),lef_count);
                lefp += lef_count;
            }
            cur_lef_p = cur_lef;
        }
        while(lef_count >0 && righ_count > 0){
            if((*cur_lef_p) == (*cur_righ_p)){
                cur_lef_p ++;
                cur_righ_p++;
                lef_count --;
                righ_count --;
            }else{
                return (*cur_lef_p) > (*cur_righ_p) ? 1 :  ((*cur_lef_p) == (*cur_righ_p) ? 0 : -1);
            }
        }
    }

    while(righp < righend && lef_count > 0){

        if(righ_count == 0){
            righ_count =  (0x1&((*righp) >> 7)) == 1 ? (int32)((*righp) & 0x7f) + THRESHOLD :  (int32)((*righp) & 0x7f);
            if((0x1&((*righp) >> 7)) == 1) {
                righp ++;
                cur_righ_p = cur_righ;
                repeat_buf_copy(cur_righ_p,(*(righp)),righ_count);
                *cur_righ_p = '\0';
                righp++;
            }else{
                righp++;
                memcpy(cur_righ,(righp),righ_count);
                *(cur_righ + righ_count) = '\0';
                righp += righ_count;
            }
            cur_righ_p = cur_righ;
        }
        while(lef_count >0 && righ_count > 0){
            if((*cur_lef_p) == (*cur_righ_p)){
                cur_lef_p ++;
                cur_righ_p++;
                lef_count --;
                righ_count --;
            }else{
                return (*cur_lef_p) > (*cur_righ_p) ? 1 :  ((*cur_lef_p) == (*cur_righ_p) ? 0 : -1);
            }
        }
    }

    if(lefp < lefend || lef_count > 0) return 1;
    else if(righp < righend || righ_count > 0) return -1;
    else return 0;

}

static int32 
hocotext_mixed_cmp(struct varlena * left, 
                    struct varlena * right, 
                    Oid collid)
{
    /*
        left is compressed and right is uncompressed.
    */
    int exchange = false;
    char * righp;
    char * righend;
    char * lefp;
    char * lefend;

    char* cur_lef = (char *)palloc(131);
    char *cur_lef_p = cur_lef;
    int lef_count = 0;

    if(!VARATT_IS_COMPRESSED(left) && VARATT_IS_COMPRESSED(right)){
        exchange = true;
        lefp = (char *) right + VARHDRSZ_COMPRESSED + 8; // jump over 8 byte index info 
        lefend = (char *) right + VARSIZE_ANY(right);
        righp = VARDATA_ANY(left);
        righend = (char *)left + VARSIZE(left);
    }else{
        lefp = (char *) left + VARHDRSZ_COMPRESSED + 8; // jump over 8 byte index info 
        lefend = (char *) left + VARSIZE_ANY(left);
        righp = VARDATA_ANY(right);
        righend = (char *)right + VARSIZE(right);
    }

	if (!OidIsValid(collid))
	{
		/*
		 * This typically means that the parser could not resolve a conflict
		 * of implicit collations, so report it that way.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which collation to use for %s function",
						"hocotext_cmp()"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}

    while(lefp < lefend && righp < righend){
        if(lef_count == 0){
            lef_count =  (0x1&((*lefp) >> 7)) == 1 ? (int)((*lefp) & 0x7f) + THRESHOLD :  (int)((*lefp) & 0x7f);
            if((0x1&((*lefp) >> 7)) == 1) {
                lefp ++;
                cur_lef_p = cur_lef;
                repeat_buf_copy(cur_lef_p,(*(lefp)),lef_count);
                *cur_lef_p = '\0';

                lefp++;
            }else{
                lefp++;
                memcpy(cur_lef,(lefp),lef_count);
                *(cur_lef + lef_count) = '\0';
                lefp += lef_count;
            }
            cur_lef_p = cur_lef;
        }


        while(lef_count >0 && righp < righend){
            if((*cur_lef_p) == (*righp)){
                cur_lef_p ++;
                lef_count --;
                righp ++;
            }else{
                return exchange ? ((*cur_lef_p) > (*righp) ? -1 :  ((*cur_lef_p) == (*righp) ? 0 : 1)) : ((*cur_lef_p) > (*righp) ? 1 :  ((*cur_lef_p) == (*righp) ? 0 : -1));
            }
        }
    }

    while(righp < righend && lef_count > 0){
        if((*cur_lef_p) == (*righp)){
            cur_lef_p ++;
            lef_count --;
            righp ++;
        }else{
            return exchange ? ((*cur_lef_p) > (*righp) ? -1 :  ((*cur_lef_p) == (*righp) ? 0 : 1)) : ((*cur_lef_p) > (*righp) ? 1 :  ((*cur_lef_p) == (*righp) ? 0 : -1));
        }
    }

    if(lefp < lefend || lef_count > 0) return exchange ? -1 : 1;
    else if(righp < righend) return exchange ? 1 : -1;
    else return 0;

}

static int32 
hocotext_common_cmp(struct varlena * left, 
                    struct varlena * right, 
                    Oid collid)
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
						"hocotext_cmp()"),
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

text * 
hocotext_hoco_extract(struct varlena * source,
                        int32 offset,
                        int32 len,
                        Oid collid){
    text *result = (text *)palloc(len + VARHDRSZ + 10); 
	const char *sp;
	// const char *srcend;
	char *dp;
	char *destend;
    int32 cur_offset = 0; //offset in raw text
    int32 count = 0;
    int32 type = 1; // 1: repeated  0: single
    int32 reach = 0;
    int32 rawsize = VARDATA_COMPRESSED_GET_EXTSIZE(source);
    // int32 source_len = 0;
	if (!OidIsValid(collid))
	{
		/*
		 * This typically means that the parser could not resolve a conflict
		 * of implicit collations, so report it that way.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which collation to use for %s function",
						"hocotext_extract()"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}



    sp = (char *) source + VARHDRSZ_COMPRESSED + 8; 
    // srcend = (char*) source + VARSIZE(source);
    // source_len = VARSIZE(source) - VARHDRSZ_COMPRESSED -8;




    if(rawsize < offset){
		ereport(ERROR,
				(errcode(ERRCODE_PLPGSQL_ERROR),
				 errmsg("Bad extraction start offset %d, total value size is only %d",
						offset, rawsize),
				 errhint("change extraction start offset")));
    }
    if(len < 0){
		ereport(ERROR,
				(errcode(ERRCODE_PLPGSQL_ERROR),
				 errmsg("Bad extraction len %d < 0",
						len),
				 errhint("change extraction len")));
    }
    
    dp = VARDATA_ANY(result);
    destend = VARDATA_ANY(result) + len;

    /**
     * locate the offset
     */
    while(cur_offset < offset || !reach){
        type = (int32)(((*sp) >> 7) & 1);
        count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
        reach = offset - (count + cur_offset) <= 0 ? 1 : 0;
        if(type == 1){
            sp += reach ? 1 : 2; 
        }else{
            sp += reach ?  offset - cur_offset + 1 : count + 1; 
        }
        count -= reach ? offset - cur_offset : 0;
        cur_offset += reach ? offset - cur_offset : count ; 
    }
    while(cur_offset - offset < len && cur_offset < rawsize){
        if(sp == source->vl_dat){
            type = (int32)(((*sp) >> 7)&0x1);
            count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
        }
        if(count + cur_offset - offset > len) count = len + offset - cur_offset;
        
        if(type == 1){
            //repeat
            repeat_buf_copy(dp,(*sp),count);
            sp ++ ;
        }else{
            //single
            memcpy(dp,sp,count);
            dp += count;
            sp += count;
        }
        cur_offset += count;
        type = (int32)(((*sp) >> 7)&0x1);
        count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
        sp++;
    }
    *dp = '\0';
    if(!(dp==destend) || cur_offset == rawsize){
        pg_printf("Error: wrong extraction result\n");
        return NULL;
    }
    SET_VARSIZE(result,len+VARHDRSZ);
    return result;
}


text * 
hocotext_hoco_extract_with_index_optimization(struct varlena * source,
                                                int32 offset,
                                                int32 len,
                                                Oid collid){
    text *result = (text *)palloc(len + VARHDRSZ + 10); 
	const char *sp;
    Oid relid = 0;
	// const char *srcend;
	char *dp;
	char *destend;
    int32 cur_offset = 0; //offset in raw text
    int32 count = 0;
    int32 type = 1; // 1: repeated  0: single
    int32 reach = 0;
    int32 rawsize = VARDATA_COMPRESSED_GET_EXTSIZE(source);
    int32 index_pointer = 0;
    index_pair location;
    // int32 source_len = 0;
	if (!OidIsValid(collid))
	{
		/*
		 * This typically means that the parser could not resolve a conflict
		 * of implicit collations, so report it that way.
		 */
		ereport(ERROR,
				(errcode(ERRCODE_INDETERMINATE_COLLATION),
				 errmsg("could not determine which collation to use for %s function",
						"hocotext_extract()"),
				 errhint("Use the COLLATE clause to set the collation explicitly.")));
	}



    sp = (char *) source + VARHDRSZ_COMPRESSED; 
    // srcend = (char*) source + VARSIZE(source);
    // source_len = VARSIZE(source) - VARHDRSZ_COMPRESSED - 8;

    /*
     * fetch raw value and get the fisrt 4 byte,
	 * which is the int32 index_pointer.
    */

	for(int seg = 0 ; seg < 4 ; seg ++,sp ++)
		index_pointer |= (int32)((*sp) & 0xFF) << (8*seg);
    
    /*
     * fetch raw value and get the next 4 byte,
	 * which is the unsigned int32 relid.
    */
	for(int seg = 0 ; seg < 4 ; seg ++,sp ++)
		relid |= (int32)((*sp) & 0xFF) << (8*seg);
    /**
     * search pg_compression_index table and 
     * pre-locate the offset segmentation.
    */
    location = searchCompressionIndex(relid,index_pointer,offset);

    if(rawsize < offset){
		ereport(ERROR,
				(errcode(ERRCODE_PLPGSQL_ERROR),
				 errmsg("Bad extraction start offset %d, total value size is only %d",
						offset, rawsize),
				 errhint("Bad extraction start offset")));
    }
    if(len < 0){
		ereport(ERROR,
				(errcode(ERRCODE_PLPGSQL_ERROR),
				 errmsg("Bad extraction len %d < 0",
						len),
				 errhint("Bad extraction len")));
    }
    
    dp = VARDATA_ANY(result);
    destend = VARDATA_ANY(result) + len;

    /**
     * locate the offset
     */
    cur_offset += location.uncompress_offset;
    sp += location.compress_offset;
    while(cur_offset < offset || !reach){
        type = (int32)(((*sp) >> 7) & 1);
        count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
        reach = offset - (count + cur_offset) <= 0 ? 1 : 0;
        if(type == 1){
            sp += reach ? 1 : 2; 
        }else{
            sp += reach ?  offset - cur_offset + 1 : count + 1; 
        }
        count -= reach ? offset - cur_offset : 0;
        cur_offset += reach ? offset - cur_offset : count ; 
    }
    while(cur_offset - offset < len && cur_offset < rawsize){
        if(sp == source->vl_dat){
            type = (int32)(((*sp) >> 7)&0x1);
            count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
        }
        if(count + cur_offset - offset > len) count = len + offset - cur_offset;
        
        if(type == 1){
            //repeat
            repeat_buf_copy(dp,(*sp),count);
            sp ++ ;
        }else{
            //single
            memcpy(dp,sp,count);
            dp += count;
            sp += count;
        }
        cur_offset += count;
        type = (int32)(((*sp) >> 7)&0x1);
        count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
        sp++;
    }
    *dp = '\0';
    if(!(dp==destend) || cur_offset == rawsize){
        pg_printf("Error: wrong extraction result\n");
        return NULL;
    }
    SET_VARSIZE(result,len+VARHDRSZ);
    return result;
}

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
						"hocotext_extract()"),
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
        result = hocotext_hoco_extract(source_str,offset,len,PG_GET_COLLATION());
    }else{
        result = hocotext_common_extract(source_str,offset,len,PG_GET_COLLATION());
    }


   PG_FREE_IF_COPY(source,0);

   PG_RETURN_TEXT_P(result);
}

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
        result = hocotext_hoco_extract_with_index_optimization(source_str,offset,len,PG_GET_COLLATION());
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
    struct varlena *source_str = (text *) pg_detoast_datum_packed_without_decompression(source);

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
    struct varlena *left_datum = (text *) pg_detoast_datum_packed_without_decompression(left);
    struct varlena *right_datum = (text *) pg_detoast_datum_packed_without_decompression(right);
    bool is_comp_lef = VARATT_IS_COMPRESSED(left_datum);
    bool is_comp_righ = VARATT_IS_COMPRESSED(right_datum);
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
	// pfree(left_datum);
	// pfree(right_datum);
	PG_FREE_IF_COPY(left, 0);
	PG_FREE_IF_COPY(right, 1);

    PG_RETURN_INT32(result);
}


Datum
hocotext_hash(PG_FUNCTION_ARGS){

    struct varlena *str = PG_GETARG_RAW_VARLENA_P(0);

    struct varlena *str_datum = (text *) pg_detoast_datum_packed_without_decompression(str);


    /**
     * do nothing
    */

    PG_RETURN_INT32(0);
}


