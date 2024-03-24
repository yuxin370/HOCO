/**
 * contrib/hocotext/hocotext_lz4.c
*/
#include "hocotext.h"

typedef struct lz4Group {
    char *literalBuffer;
    int32 literalLength;
    int32 offsetLength;
    int32 matchLength;
    int32 startOffset;
}lz4Group;
typedef struct groupPair {
    lz4Group group;
    char* p;
}groupPair;

#define GroupVolume 200

groupPair hocotext_lz4_get_next_group(char *sp,char *srcend);
int32 hocotext_lz4_hoco_extract_handler(char *dp, lz4Group *decompGroup, int32 startGroup, int32 offset, int32 len);


text * 
hocotext_lz4_hoco_decompression(struct varlena * source,int32 offset,int32 len,Oid collid){
	const char *sp;
	const char *srcend;
    int32 rawsize = VARDATA_COMPRESSED_GET_EXTSIZE(source);
    text *rawdata = (text *)palloc(rawsize + VARHDRSZ);
    int32 literal_length = 0;
    int32 match_length = 0;
    int32 offset_length = 0;
    char *rp;
    const char * rawend;
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
    // source_len = VARSIZE(source) - VARHDRSZ_COMPRESSED;



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

    rp = VARDATA_ANY(rawdata);
    rawend = (char*) rp + VARSIZE(rawdata);



    
    while(sp < srcend){

        /**
         * read the first byte [token]
        */
        literal_length = ((*sp) >> 4)&0x0f;
        match_length = (*sp)&0x0f;
        /**
         * read the following literal byte [optinal]
        */
        if(literal_length == 0x0f){
            do{
                sp ++;
                literal_length += ((*sp)&0xff);
            }while(((*sp)&0xff) == 0xff);
        }

        /**
         * buffer the follwing literal bytes [optinal]
        */
        sp ++;
        while(literal_length > 0){
            *(rp) = *(sp);
            rp ++;
            sp ++;
            literal_length --;
        }
        
        /**
         * check if it's the end
        */
        if(!(sp < srcend)) break;


        /**
         * read the offset_length
        */
        offset_length = (*sp) & 0xff;
        sp ++;
        offset_length = offset_length | (((*sp)) << 8 & 0xff00);
    
        /**
         * read the following match_length [optinal]
        */
        if(match_length == 0x0f){
            do{
                sp ++;
                match_length += ((*sp)&0xff);
            }while(((*sp)&0xff) == 0xff);
        }
        
        match_length += 4;
        sp ++;

        /**
         * buffer the matched bytes
        */
        while(offset_length < match_length){
            memcpy(rp,(rp-offset_length),offset_length);
            match_length -= offset_length;
            rp += offset_length;
        }
        memcpy(rp,(rp-offset_length),match_length);
        rp += match_length;
    }

    *rp = '\0';
    // elog(INFO,"rawdata = %s",(char*)VARDATA_ANY(rawdata));

    if(rp != rawend){
        elog(ERROR,"Wrong decompression result size!");
        return NULL; // keep the compiler silent.
    }
    SET_VARSIZE(rawdata,rawsize+VARHDRSZ);
    return rawdata;
}


/**
 * hocotext_lz4_get_next_group
 * decompress a group of token.
 * 
*/
groupPair 
hocotext_lz4_get_next_group(char *sp,char *srcend){
    int32 literal_length = 0;
    int32 match_length = 0;
    int32 offset_length = 0;
    lz4Group resultGroup;
    groupPair resultPair;
    char *literalBuffer = (char *)palloc(sizeof(char) * GroupVolume *10);
    char *rp = literalBuffer;
    /**
     * considering the following example:
     * hellohelloabcdefabcdefzzzbcdef    hello (5,5) abcdef (6,6) zzz (8,5)
     * helloheeloabcdefabcdezzzabcde     hello (5,5) abcdef (6,5) zzz (8,5)
    */
         
    if(sp < srcend){
        /**
         * read the first byte [token]
        */
        literal_length = ((*sp) >> 4)&0x0f;
        match_length = (*sp)&0x0f;
        /**
         * read the following literal byte [optinal]
        */
        if(literal_length == 0x0f){
            do{
                sp ++;
                literal_length += ((*sp)&0xff);
            }while(((*sp)&0xff) == 0xff);
        }
        resultGroup.literalLength = literal_length;

        /**
         * buffer the follwing literal bytes [optinal]
        */
        sp ++;
        while(literal_length > 0){
            *(rp) = *(sp);
            rp ++;
            sp ++;
            literal_length --;
        }
        
        /**
         * check if it's the end
        */
        if((sp < srcend)){

            /**
             * read the offset_length
            */
            offset_length = (*sp) & 0xff;
            sp ++;
            offset_length = offset_length | (((*sp)) << 8 & 0xff00);
            resultGroup.offsetLength = offset_length;
            /**
             * read the following match_length [optinal]
            */
            if(match_length == 0x0f){
                do{
                    sp ++;
                    match_length += ((*sp)&0xff);
                }while(((*sp)&0xff) == 0xff);
            }
            
            match_length += 4;

            resultGroup.matchLength = match_length;
            sp ++;

            // /**
            //  * buffer the matched bytes
            // */
            // while(offset_length < match_length){
            //     memcpy(rp,(rp-offset_length),offset_length);
            //     match_length -= offset_length;
            //     rp += offset_length;
            // }
            // memcpy(rp,(rp-offset_length),match_length);
            // rp += match_length;
        }else{
            resultGroup.offsetLength = 0;
            resultGroup.matchLength = 0;
        }
    }
    *rp = '\0';
    if(resultGroup.literalLength == 0){
        pfree(literalBuffer);
        resultGroup.literalBuffer = NULL;
    }else{
        resultGroup.literalBuffer = literalBuffer;
    }
    resultPair.group = resultGroup;
    resultPair.p = sp;
    return resultPair;
}

int32
hocotext_lz4_hoco_cmp(struct varlena * left, struct varlena * right, Oid collid){
    // char *lefp;
    // char *lefend;
	// char *righp;
    // char *righend;

    // int32 rawsize_lef = VARDATA_COMPRESSED_GET_EXTSIZE(left);
    // int32 rawsize_righ = VARDATA_COMPRESSED_GET_EXTSIZE(right);

    // int32 groupCount_lef = 0;
    // int32 groupCount_righ = 0;
    // int32 current_lef = 0;
    // int32 current_righ = 0;

    // // char* lef_buffer = (char *)palloc(rawsize_lef);
    // // char* righ_buffer = (char *)palloc(rawsize_righ);
    // // char *cur_lef_p = lef_buffer;
    // // char *cur_righ_p = righ_buffer;

    // lz4Group *lefGroup = (lz4Group *)palloc(sizeof(lz4Group)*GroupVolume);
    // lz4Group *righGroup = (lz4Group *)palloc(sizeof(lz4Group)*GroupVolume); // alloc groupvolume

	// if (!OidIsValid(collid))
	// {
	// 	/*
	// 	 * This typically means that the parser could not resolve a conflict
	// 	 * of implicit collations, so report it that way.
	// 	 */
	// 	ereport(ERROR,
	// 			(errcode(ERRCODE_INDETERMINATE_COLLATION),
	// 			 errmsg("could not determine which collation to use for %s function",
	// 					"hocotext_rle_cmp()"),
	// 			 errhint("Use the COLLATE clause to set the collation explicitly.")));
	// }

    // lefp = (char *) left + VARHDRSZ_COMPRESSED; 
    // righp = (char *) right + VARHDRSZ_COMPRESSED;
    // lefend = (char *) left + VARSIZE_ANY(left);
    // righend = (char *) right + VARSIZE_ANY(right);

    // int32 idx = 0;
    // while(lefp < lefend && righp < righend ){
    //     lefGroup[current_lef] = hocotext_lz4_get_next_group(lefp,lefend);
    //     righGroup[current_righ] = hocotext_lz4_get_next_group(righp,righend);
    //     while(idx <= lefGroup->literalLength && idx <= righGroup->literalLength){
    //         if(lefGroup->literalBuffer[idx] == righGroup->literalBuffer[idx]){
    //             idx ++;
    //             continue;
    //         }else{
    //             return lefGroup->literalBuffer[idx] > righGroup->literalBuffer[idx] ? 1 : -1; 
    //         }
    //     }
    //     if(idx == lefGroup->literalLength && idx < righGroup->literalLength){
    //         // locateBuffer
    //     }else if(idx < lefGroup->literalLength && idx == righGroup->literalLength){

    //     }else{
    //         if(lefGroup->matchLength == righGroup->matchLength && lefGroup->offsetLength == righGroup->offsetLength){

    //         }
    //     }

    //     break;  
    // }

    return 1;

}


/**
 * given decompGroup, do the real extract work.
*/
int32
hocotext_lz4_hoco_extract_handler(char *dp, lz4Group *decompGroup, int32 startGroup, int32 offset, int32 len){
    lz4Group token;
    int32 cpySize = 0;
    char *sp = dp;

    int32 matchLocation = 0;
    int32 matchStart = 0;
    int32 matchLength = 0;
    int32 matchOffset = 0;

    int32 tmplen;
    int32 curEnd;

    while(len > 0){
        token = decompGroup[startGroup];
        /**
         * extract in the literal buffer
        */
        cpySize = token.literalLength - (offset - token.startOffset) >= len ? len : token.literalLength - (offset - token.startOffset);
        // elog(INFO,"offset = %d len = %d \n startGroup = %d decompGroup.startOffset = %d decompGroup.literalLength = %d decompGroup.offsetLength = %d decompGroup.matchLength = %d",
        //         offset,
        //         len,
        //         startGroup,
        //         token.startOffset,
        //         token.literalLength,
        //         token.offsetLength,
        //         token.matchLength);
        if(cpySize > 0){
            memcpy(dp,token.literalBuffer+(offset - token.startOffset),cpySize);
            dp += cpySize;
            offset += cpySize;
            len -= cpySize;
            if(len == 0) break;
        }

        /**
         * extract in the match buffer
        */
        matchLocation = startGroup;
        curEnd = decompGroup[matchLocation].startOffset + decompGroup[matchLocation].literalLength + decompGroup[matchLocation].matchLength;
        
        if(offset >= curEnd){
            // elog(INFO,"forward!");
            /**
             * search forwards
            */
            matchOffset = offset;
            matchLength = len;
            while(curEnd <= matchOffset){
                matchLocation ++;
                curEnd = decompGroup[matchLocation].startOffset + decompGroup[matchLocation].literalLength + decompGroup[matchLocation].matchLength;
            }
        }else{
            // elog(INFO,"backward!");

            /**
             * search backwards
            */
            matchStart = token.startOffset + token.literalLength - token.offsetLength;
            matchOffset = offset - token.offsetLength;
            while(decompGroup[matchLocation].startOffset >= matchStart && matchLocation > 0){
                matchLocation --;
            }

            matchLength = offset + len <= token.startOffset + token.literalLength + token.matchLength ? 
                            len : 
                            token.startOffset + token.literalLength + token.matchLength - offset;
        }
        tmplen = hocotext_lz4_hoco_extract_handler(dp,decompGroup,matchLocation,matchOffset,matchLength);
        dp += tmplen;
        offset += matchLength;
        len -= matchLength;
        startGroup =  startGroup >= matchLocation ? startGroup +1 : matchLocation + 1;
    }
    *dp = '\0';
    return dp - sp;
}


/**
 * hocotext_lz4_hoco_extract
 * 
 * construct the result using the compressed information
*/
text * 
hocotext_lz4_hoco_extract(struct varlena * source,int32 offset,int32 len,Oid collid){
    int32 rawsize = VARDATA_COMPRESSED_GET_EXTSIZE(source);
    /**
     * cut off the extraction length
    */


    char *sp = (char *) source + VARHDRSZ_COMPRESSED; 
	char *srcend = (char*) source + VARSIZE(source);
    // int32 source_len = VARSIZE(source) - VARHDRSZ_COMPRESSED;


    int32 allocedVolume = GroupVolume;
    lz4Group *decompGroup = (lz4Group *)palloc(sizeof(lz4Group)*allocedVolume*10);
    int32 currentGroup = 0;
    // int32 currentoffset = 0;

    int32 startOffsetGroup = 0;
    int32 curEnd = 0;

    int32 tmplen = 0;

    groupPair tmpPair;

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

    if(rawsize < offset){
		// ereport(ERROR,
		// 		(errcode(ERRCODE_PLPGSQL_ERROR),
		// 		 errmsg("Bad extraction start offset %d, total value size is only %d",
		// 				offset, rawsize),
		// 		 errhint("change extraction start offset")));
        offset = rawsize;
    }
    len = rawsize >= offset + len ? len : rawsize - offset;

    if(len < 0){
		ereport(ERROR,
				(errcode(ERRCODE_PLPGSQL_ERROR),
				 errmsg("Bad extraction len %d < 0",
						len),
				 errhint("change extraction len")));
    }
    
    // elog(INFO,"rawsize = %d , offset = %d, len = %d",rawsize,offset,len);

    text *result = (text *)palloc(len + VARHDRSZ); 
    lz4Group token;

	char *dp = VARDATA_ANY(result);
    char *ds = dp;
	char *destend = VARDATA_ANY(result) + len;

    /**
     * construct the decompGroup and locate the start location of the offset 
    */
    while(sp <= srcend && dp == ds){
        tmpPair = hocotext_lz4_get_next_group(sp,srcend);
        decompGroup[currentGroup] = tmpPair.group;
        sp = tmpPair.p;
        decompGroup[currentGroup].startOffset = currentGroup > 0 ?  
                                                decompGroup[currentGroup - 1].startOffset + decompGroup[currentGroup-1].literalLength + decompGroup[currentGroup-1].matchLength :
                                                0;
        curEnd = decompGroup[currentGroup].startOffset + decompGroup[currentGroup].literalLength + decompGroup[currentGroup].matchLength;
        if(offset <= curEnd){
            startOffsetGroup = currentGroup;
            if(offset + len <= curEnd){
                tmplen = hocotext_lz4_hoco_extract_handler(dp,decompGroup, startOffsetGroup,offset,len);
                dp += tmplen;
            }
            currentGroup ++;
            break;
        }
        currentGroup++;
        // if(currentGroup > allocedVolume ){
        //     allocedVolume += GroupVolume;
        //     decompGroup = (lz4Group *)realloc(decompGroup , sizeof(lz4Group)*allocedVolume);
        // }
        
    }


    /**
     * construct the decompGroup till get the end location of the offset 
    */

    while(dp == ds && sp <= srcend){
        tmpPair = hocotext_lz4_get_next_group(sp,srcend);
        decompGroup[currentGroup] = tmpPair.group;
        sp = tmpPair.p;
        decompGroup[currentGroup].startOffset = currentGroup > 0 ?  
                                                decompGroup[currentGroup - 1].startOffset + decompGroup[currentGroup-1].literalLength + decompGroup[currentGroup-1].matchLength :
                                                0;
        curEnd = decompGroup[currentGroup].startOffset + decompGroup[currentGroup].literalLength + decompGroup[currentGroup].matchLength;
        if(offset + len <= curEnd){
            tmplen = hocotext_lz4_hoco_extract_handler(dp,decompGroup, startOffsetGroup,offset,len);
            dp += tmplen;
            break;
        }
        currentGroup ++;
        // if(currentGroup > allocedVolume ){
        //     allocedVolume += GroupVolume;
        //     decompGroup = (lz4Group *)realloc(decompGroup , sizeof(lz4Group)*allocedVolume);
        // }
    }
    *dp = '\0';

    if(!(dp==destend)){
        pg_printf("Error: wrong extraction result\n");
        return NULL;
    }

    SET_VARSIZE(result,len+VARHDRSZ);

    for(int i = 0 ; i <= currentGroup; i ++){
        // elog(INFO,"pfreeing %d, [total = %d] length = %d buffer = %s offset = %d match = %d",
        //         i,
        //         currentGroup,
        //         decompGroup[i].literalLength,
        //         decompGroup[i].literalBuffer,
        //         decompGroup[i].offsetLength,
        //         decompGroup[i].matchLength);
        if(decompGroup[i].literalLength!= 0&& decompGroup[i].literalBuffer != NULL){
            pfree(decompGroup[i].literalBuffer);
        }
    }
    pfree(decompGroup);
    // elog(INFO,"in result = %s",VARDATA_ANY(result));

    return result;
}




/**
 * hocotext_lz4_hoco_extract_naive
 * 
 * decompress and stop as long as get the result.
*/

text * 
hocotext_lz4_hoco_extract_naive(struct varlena * source,int32 offset,int32 len,Oid collid){
    char *sp = (char *) source + VARHDRSZ_COMPRESSED; 
	char *srcend = (char*) source + VARSIZE(source);
    int32 source_len = VARSIZE(source) - VARHDRSZ_COMPRESSED;
    int32 rawsize = VARDATA_COMPRESSED_GET_EXTSIZE(source);
    text *rawdata = (text *)palloc(rawsize + VARHDRSZ);
    /**
     * cut off the extraction length
    */
    
    // elog(INFO,"rawsize = %d , offset = %d, len = %d",rawsize,offset,len);

    char *rp = VARDATA_ANY(rawdata);
    char *rsp = rp;
    char *rendp = rp + offset + len;
    
    int32 literal_length = 0;
    int32 match_length = 0;
    int32 offset_length = 0;

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

    if(rawsize < offset){
		// ereport(ERROR,
		// 		(errcode(ERRCODE_PLPGSQL_ERROR),
		// 		 errmsg("Bad extraction start offset %d, total value size is only %d",
		// 				offset, rawsize),
		// 		 errhint("change extraction start offset")));
        offset = rawsize;
    }
    len = rawsize >= offset + len ? len : rawsize - offset;

    if(len < 0){
		ereport(ERROR,
				(errcode(ERRCODE_PLPGSQL_ERROR),
				 errmsg("Bad extraction len %d < 0",
						len),
				 errhint("change extraction len")));
    }
    text *result = (text *)palloc(len + VARHDRSZ ); 

	char *dp = VARDATA_ANY(result);
	char *destend = VARDATA_ANY(result) + len;
    while(sp < srcend && rp < rendp){

        /**
         * read the first byte [token]
        */
        literal_length = ((*sp) >> 4)&0x0f;
        match_length = (*sp)&0x0f;
        /**
         * read the following literal byte [optinal]
        */
        if(literal_length == 0x0f){
            do{
                sp ++;
                literal_length += ((*sp)&0xff);
            }while(((*sp)&0xff) == 0xff);
        }

        /**
         * buffer the follwing literal bytes [optinal]
        */
        sp ++;
        while(literal_length > 0){
            *(rp) = *(sp);
            rp ++;
            sp ++;
            literal_length --;
        }
        
        /**
         * check if it's the end
        */
        if(!(sp < srcend  && rp < rendp)) break;


        /**
         * read the offset_length
        */
        offset_length = (*sp) & 0xff;
        sp ++;
        offset_length = offset_length | (((*sp)) << 8 & 0xff00);
    
        /**
         * read the following match_length [optinal]
        */
        if(match_length == 0x0f){
            do{
                sp ++;
                match_length += ((*sp)&0xff);
            }while(((*sp)&0xff) == 0xff);
        }
        
        match_length += 4;
        sp ++;

        /**
         * buffer the matched bytes
        */
        while(offset_length < match_length){
            memcpy(rp,(rp-offset_length),offset_length);
            match_length -= offset_length;
            rp += offset_length;
            if(!(rp < rendp)) break;
        }

        if(offset_length >= match_length){
            memcpy(rp,(rp-offset_length),match_length);
            rp += match_length;
        }
    }

    *rp = '\0';



    memcpy(dp,rsp + offset,len);
    dp += len;
    *dp = '\0';

    if(!(dp==destend)){
        pg_printf("Error: wrong extraction result\n");
        return NULL;
    }
    SET_VARSIZE(result,len+VARHDRSZ);
    pfree(rawdata);
    return result;
}


