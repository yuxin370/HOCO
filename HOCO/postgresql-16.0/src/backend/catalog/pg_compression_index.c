/*-------------------------------------------------------------------------
 *
 * pg_compression_index.c
 *	  routines to support manipulation of the pg_compression_index relation
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *  Yuxin Tang
 *  2023.12.2
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_compression_index.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_compression_index.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"


bool 
HasCompressionIndex(Oid relid, int32 rowid){
    bool find = false;
    Relation catalogRelation;
    ScanKeyData key;
    SysScanDesc scan;
    HeapTuple	compressionIndexTuple;

    /**
     * Find pg_compression_index entries by relid and rowid.
    */

    if (!OidIsValid(relid)){
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                errmsg("relation does not exist")));
        return false;
    }
    catalogRelation = table_open(CompressionIndexRelationId,RowExclusiveLock);
    ScanKeyInit(&key,
                Anum_pg_compression_index_relid,
                BTEqualStrategyNumber, F_OIDEQ,
                ObjectIdGetDatum(relid));
    scan = systable_beginscan(catalogRelation,CompressionIndexSourceLocationIndexId,
                                true, NULL, 1, &key);

	while (HeapTupleIsValid(compressionIndexTuple = systable_getnext(scan)))
    {
        Form_pg_compression_index compForm;
        Oid cur_relid;
        int32 cur_rowid;
        compForm = (Form_pg_compression_index) GETSTRUCT(compressionIndexTuple);
        cur_relid = compForm->relid;
        cur_rowid = compForm->rowid;
        if(cur_relid == relid && cur_rowid == rowid){
            find = true;
            break;
        }
    }

    /** Done*/
    systable_endscan(scan);
	table_close(catalogRelation, RowExclusiveLock);

    return find;
}



index_pair
searchCompressionIndex(Oid relid,int32 rowid,int32 uncompress_off)
{
    Relation catalogRelation;
    ScanKeyData key;
    SysScanDesc scan;
    HeapTuple	compressionIndexTuple;
    index_pair location_res;
    
    location_res.compress_offset = 0;
    location_res.uncompress_offset = 0;

    /**
     * Find pg_compression_index entries by relid and rowid.
    */

    if (!OidIsValid(relid)){
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                errmsg("relation does not exist")));
        return location_res;
    }
    catalogRelation = table_open(CompressionIndexRelationId,RowExclusiveLock);
    ScanKeyInit(&key,
                Anum_pg_compression_index_relid,
                BTEqualStrategyNumber, F_OIDEQ,
                ObjectIdGetDatum(relid));
    scan = systable_beginscan(catalogRelation,CompressionIndexSourceLocationIndexId,
                                true, NULL, 1, &key);

	while (HeapTupleIsValid(compressionIndexTuple = systable_getnext(scan)))
    {
        Form_pg_compression_index compForm;
        Oid cur_relid;
        int32 cur_rowid;
        int32 cur_comp_off;
        int32 cur_uncomp_off;
        compForm = (Form_pg_compression_index) GETSTRUCT(compressionIndexTuple);
        cur_relid = compForm->relid;
        cur_rowid = compForm->rowid;
        cur_comp_off = compForm->compress_off;
        cur_uncomp_off = compForm->uncompress_off;
        if(cur_relid == relid && cur_rowid == rowid){
            if(cur_uncomp_off > location_res.uncompress_offset && cur_uncomp_off <= uncompress_off){
                location_res.compress_offset = cur_comp_off;
                location_res.uncompress_offset = cur_uncomp_off;   
            }else if(cur_uncomp_off > uncompress_off){
                break;
            }
        }
    }

    /** Done*/
    systable_endscan(scan);
	table_close(catalogRelation, RowExclusiveLock);
    return location_res;
}

/**
 * insert a single pg_compression_index row with the given data
 * 
 * relid may be invalid, in which case no tuple will be inserted.
 * 
 */
void 
InsertCompressionIndex(Oid relid, int32 rowid,
                        int32 compress_off, int32 uncompress_off)
{
    Datum values[Natts_pg_compression_index];
    bool nulls[Natts_pg_compression_index];
    HeapTuple tuple;
    Relation compressionIndexRelation;

	if (!OidIsValid(relid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("relation does not exist")));
    else{

        compressionIndexRelation = table_open(CompressionIndexRelationId,RowExclusiveLock);

        memset(nulls,0,sizeof(nulls));
        memset(values,0,sizeof(values));

        /**
         * Make the pg_compression_index entry
        */
        values[Anum_pg_compression_index_relid-1] = ObjectIdGetDatum(relid);
        values[Anum_pg_compression_index_rowid-1] = Int32GetDatum(rowid);
        values[Anum_pg_compression_index_compress_off-1] = Int32GetDatum(compress_off);
        values[Anum_pg_compression_index_uncompress_off-1] = Int32GetDatum(uncompress_off);

        tuple = heap_form_tuple(RelationGetDescr(compressionIndexRelation),values,nulls);

        CatalogTupleInsert(compressionIndexRelation,tuple);

        heap_freetuple(tuple);

        table_close(compressionIndexRelation,RowExclusiveLock);
    }

}


/**
 * UpdateCompressionIndex
 * 
 * Update the compression offset and uncompression offset for the given table
 * we suppose our index is constructed by evenly dividing the compressed offset
 * 
*/
void 
UpdateCompressionIndex(Oid relid, int32 rowid,
                        int32 compress_off, int32 uncompress_off)
{
    Relation rel;
    HeapTuple tup;
	bool		nulls[Natts_pg_compression_index];
	Datum		values[Natts_pg_compression_index];
	bool		replaces[Natts_pg_compression_index];

	rel = table_open(CompressionIndexRelationId, RowExclusiveLock);

    /**
     * Try finding existing mapping. 
    */
   tup = SearchSysCacheCopy3(COMPRESSIONINDEXSOURCELOCATION,
							  ObjectIdGetDatum(relid),
							  Int32GetDatum(rowid),
                              Int32GetDatum(compress_off));
    if (!HeapTupleIsValid(tup))
		elog(ERROR, "subscription for table %u in index pointer %d dose not exist",
			 relid, rowid);
    
    /** Update the tuple*/
	memset(values, 0, sizeof(values));
	memset(nulls, false, sizeof(nulls));
	memset(replaces, false, sizeof(replaces));


    replaces[Anum_pg_compression_index_compress_off-1] = true;
    values[Anum_pg_compression_index_compress_off-1] = Int32GetDatum(uncompress_off);

	tup = heap_modify_tuple(tup, RelationGetDescr(rel), values, nulls,
							replaces);

	/* Update the catalog. */
	CatalogTupleUpdate(rel, &tup->t_self, tup);

	/* Cleanup. */
	table_close(rel, RowExclusiveLock);
	// table_close(rel, NoLock);
}

/*
 * DeleteCompressionIndex
 *
 * Delete pg_compression_index tuples with the given relid and rowid.  
 * relid or rowid may be invalidOid, in which case no tuple will be deleted.
 * otherwise only delete tuples with the specified inhparent.
 * 
 * Returns whether at least one row was deleted.
 */
bool 
DeleteCompressionIndex(Oid relid, int32 rowid)
{
    bool deleted = false;
    Relation catalogRelation;
    ScanKeyData key;
    SysScanDesc scan;
    HeapTuple	compressionIndexTuple;

    /**
     * Find pg_compression_index entries by relid and rowid.
    */

    if (!OidIsValid(relid))
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                errmsg("relation does not exist")));
    catalogRelation = table_open(CompressionIndexRelationId,RowExclusiveLock);
    ScanKeyInit(&key,
                Anum_pg_compression_index_relid,
                BTEqualStrategyNumber, F_OIDEQ,
                ObjectIdGetDatum(relid));
    scan = systable_beginscan(catalogRelation,CompressionIndexSourceLocationIndexId,
                                true, NULL, 1, &key);

	while (HeapTupleIsValid(compressionIndexTuple = systable_getnext(scan)))
    {
        Form_pg_compression_index compForm;
        Oid cur_relid;
        int32 cur_rowid;
        compForm = (Form_pg_compression_index) GETSTRUCT(compressionIndexTuple);
        cur_relid = compForm->relid;
        cur_rowid = compForm->rowid;
        if(cur_relid == relid && cur_rowid == rowid){
			CatalogTupleDelete(catalogRelation, &compressionIndexTuple->t_self);
            deleted = true;
        }
    }

    /** Done*/
    systable_endscan(scan);
	table_close(catalogRelation, RowExclusiveLock);

    return deleted;
}