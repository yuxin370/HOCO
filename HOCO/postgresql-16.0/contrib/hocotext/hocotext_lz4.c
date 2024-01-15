/**
 * contrib/hocotext/hocotext_lz4.c
*/
#include "hocotext.h"

text * 
hocotext_lz4_hoco_extract(struct varlena * source,int32 offset,int32 len,Oid collid){
    text *result = (text *)palloc(len + VARHDRSZ + 10); 
	const char *sp;
	const char *srcend;
	char *dp;
	char *destend;
    int32 cur_offset = 0; //offset in raw text
    int32 count = 0;
    int32 type = 1; // 1: repeated  0: single
    int32 reach = 0;
    int32 rawsize = VARDATA_COMPRESSED_GET_EXTSIZE(source);
    int32 source_len = 0;
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
    srcend = (char*) source + VARSIZE(source);
    source_len = VARSIZE(source) - VARHDRSZ_COMPRESSED;




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

    elog(INFO,"rawsize = %d, source_len = %d, len = %d",rawsize,source_len,len);
    int t = 0;
    while(sp < srcend){
        t++;
        elog(INFO,"%d %c",t,(*sp));
        sp ++;
    }
    for(int  i= 0; i < len; i ++){
        *dp = 'a';
        dp ++;
    }

    // memcpy(dp,sp,len);
    // /**
    //  * locate the offset
    //  */
    // while(cur_offset < offset || !reach){
    //     type = (int32)(((*sp) >> 7) & 1);
    //     count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
    //     reach = offset - (count + cur_offset) <= 0 ? 1 : 0;
    //     if(type == 1){
    //         sp += reach ? 1 : 2; 
    //     }else{
    //         sp += reach ?  offset - cur_offset + 1 : count + 1; 
    //     }
    //     count -= reach ? offset - cur_offset : 0;
    //     cur_offset += reach ? offset - cur_offset : count ; 
    // }
    // while(cur_offset - offset < len && cur_offset < rawsize){
    //     if(sp == source->vl_dat){
    //         type = (int32)(((*sp) >> 7)&0x1);
    //         count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
    //     }
    //     if(count + cur_offset - offset > len) count = len + offset - cur_offset;
        
    //     if(type == 1){
    //         //repeat
    //         repeat_buf_copy(dp,(*sp),count);
    //         sp ++ ;
    //     }else{
    //         //single
    //         memcpy(dp,sp,count);
    //         dp += count;
    //         sp += count;
    //     }
    //     cur_offset += count;
    //     type = (int32)(((*sp) >> 7)&0x1);
    //     count = type == 1 ? (int32)((*sp) & 0x7f) + THRESHOLD : (int32)((*sp) & 0x7f);
    //     sp++;
    // }
    // *dp = '\0';
    // if(!(dp==destend) || cur_offset == rawsize){
    //     pg_printf("Error: wrong extraction result\n");
    //     return NULL;
    // }
    SET_VARSIZE(result,len+VARHDRSZ);
    return result;
}
