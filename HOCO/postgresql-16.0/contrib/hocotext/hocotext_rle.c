/**
 * contrib/hocotext/hocotext_rle.c
*/
#include "hocotext.h"

int32 
hocotext_rle_hoco_cmp(struct varlena * left, 
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
						"hocotext_rle_cmp()"),
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

int32 
hocotext_rle_mixed_cmp(struct varlena * left, 
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
						"hocotext_rle_cmp()"),
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


text * 
hocotext_rle_hoco_extract(struct varlena * source,
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
						"hocotext_rle_extract()"),
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
hocotext_rle_hoco_extract_with_index_optimization(struct varlena * source,
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
						"hocotext_rle_extract()"),
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
    /**
     * extract
    */
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
hocotext_rle_hoco_insert(struct varlena * source,
                        int32 offset,
                        text *str){
    text *result;
    return result;
}
text * 
hocotext_rle_hoco_delete(struct varlena * source,
                        int32 offset,
                        int32 len){
    text *result;
    return result;
}
