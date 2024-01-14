/*-------------------------------------------------------------------------
 *
 * pg_max_comp_index_pointer.c
 *	  routines to support manipulation of the pg_max_comp_index_pointer relation
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *  Yuxin Tang
 *  2024.1.1
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_max_comp_index_pointer.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_max_comp_index_pointer.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"


bool 
HasMaxCompIndexPointer(Oid relid){
    bool find = false;
    Relation catalogRelation;
    ScanKeyData key;
    SysScanDesc scan;
    HeapTuple	MaxCompIndexPointerTuple;

    /**
     * Find pg_max_comp_index_pointer entries by relid.
    */

    if (!OidIsValid(relid)){
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                errmsg("relation does not exist")));
        return false;
    }
    catalogRelation = table_open(MaxCompIndexPointerRelationId,RowExclusiveLock);
    ScanKeyInit(&key,
                Anum_pg_max_comp_index_pointer_relid,
                BTEqualStrategyNumber, F_OIDEQ,
                ObjectIdGetDatum(relid));
    scan = systable_beginscan(catalogRelation,MaxCompIndexPointerSourceLocationIndexId,
                                true, NULL, 1, &key);

	while (HeapTupleIsValid(MaxCompIndexPointerTuple = systable_getnext(scan)))
    {
        Form_pg_max_comp_index_pointer compForm;
        Oid cur_relid;
        compForm = (Form_pg_max_comp_index_pointer) GETSTRUCT(MaxCompIndexPointerTuple);
        cur_relid = compForm->relid;
        if(cur_relid == relid){
            find = true;
            break;
        }
    }

    /** Done*/
    systable_endscan(scan);
	table_close(catalogRelation, RowExclusiveLock);

    return find;
}


/**
 * FindMaxCompIndexPointer
 * 
 * given reild, find the max pointer recorded in te max_comp_index_pointer table
 * if there are no coressponding tuple, return 0;
 * else return the recorded maxp.
*/

int32 
FindMaxCompIndexPointer(Oid relid){
    Relation catalogRelation;
    ScanKeyData key;
    SysScanDesc scan;
    int32 res = 0;
    HeapTuple	MaxCompIndexPointerTuple;

    /**
     * Find pg_max_comp_index_pointer entries by relid.
    */

    if (!OidIsValid(relid)){
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                errmsg("relation does not exist")));
        return false;
    }
    catalogRelation = table_open(MaxCompIndexPointerRelationId,RowExclusiveLock);
    ScanKeyInit(&key,
                Anum_pg_max_comp_index_pointer_relid,
                BTEqualStrategyNumber, F_OIDEQ,
                ObjectIdGetDatum(relid));
    scan = systable_beginscan(catalogRelation,MaxCompIndexPointerSourceLocationIndexId,
                                true, NULL, 1, &key);

	while (HeapTupleIsValid(MaxCompIndexPointerTuple = systable_getnext(scan)))
    {
        Form_pg_max_comp_index_pointer compForm;
        Oid cur_relid;
        int32 cur_maxp;
        compForm = (Form_pg_max_comp_index_pointer) GETSTRUCT(MaxCompIndexPointerTuple);
        cur_relid = compForm->relid;
        cur_maxp = compForm->maxp;
        if(cur_relid == relid){
            res = cur_maxp;
            break;
        }
    }

    /** Done*/
    systable_endscan(scan);
	table_close(catalogRelation, RowExclusiveLock);

    return res;
}


/**
 * InsertMaxCompIndexPointer
 * 
 * insert a single pg_max_comp_index_pointer row with the given data
 * relid may be invalid, in which case no tuple will be inserted.
 * 
 */
void 
InsertMaxCompIndexPointer(Oid relid, int32 maxp)
{
    Datum values[Natts_pg_max_comp_index_pointer];
    bool nulls[Natts_pg_max_comp_index_pointer];
    HeapTuple tuple;
    Relation MaxCompIndexPointerRelation;

	if (!OidIsValid(relid))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_TABLE),
				 errmsg("relation does not exist")));
    else{

        MaxCompIndexPointerRelation = table_open(MaxCompIndexPointerRelationId,RowExclusiveLock);

        memset(nulls,0,sizeof(nulls));
        memset(values,0,sizeof(values));

        /**
         * Make the pg_max_comp_index_pointer entry
        */
        values[Anum_pg_max_comp_index_pointer_relid-1] = ObjectIdGetDatum(relid);
        values[Anum_pg_max_comp_index_pointer_maxp-1] = Int32GetDatum(maxp);

        tuple = heap_form_tuple(RelationGetDescr(MaxCompIndexPointerRelation),values,nulls);

        CatalogTupleInsert(MaxCompIndexPointerRelation,tuple);

        heap_freetuple(tuple);

        table_close(MaxCompIndexPointerRelation,RowExclusiveLock);
    }

}


/**
 * UpdateMaxCompIndexPointer
 * 
 * Update the compression offset and uncompression offset for the given table
 * we suppose our index is constructed by evenly dividing the compressed offset
 * 
*/
void 
UpdateMaxCompIndexPointer(Oid relid, int32 maxp)
{
    Relation rel;
    HeapTuple tup;
	bool		nulls[Natts_pg_max_comp_index_pointer];
	Datum		values[Natts_pg_max_comp_index_pointer];
	bool		replaces[Natts_pg_max_comp_index_pointer];

	rel = table_open(MaxCompIndexPointerRelationId, RowExclusiveLock);

    /**
     * Try finding existing mapping. 
    */
   tup = SearchSysCacheCopy1(MAXCOMPINDEXPOINTERSOURCELOCATION,
							  ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(tup))
		elog(ERROR, "subscription for table %u dose not exist",
			 relid);
    
    /** Update the tuple*/
	memset(values, 0, sizeof(values));
	memset(nulls, false, sizeof(nulls));
	memset(replaces, false, sizeof(replaces));

    replaces[Anum_pg_max_comp_index_pointer_maxp-1] = true;
    values[Anum_pg_max_comp_index_pointer_maxp-1] = Int32GetDatum(maxp);

	tup = heap_modify_tuple(tup, RelationGetDescr(rel), values, nulls,
							replaces);

	/* Update the catalog. */
	CatalogTupleUpdate(rel, &tup->t_self, tup);

	/* Cleanup. */
	table_close(rel, NoLock);
}

/*
 * DeleteMaxCompIndexPointer
 *
 * Delete pg_max_comp_index_pointer tuples with the given relid and maxp.  
 * relid or maxp may be invalidOid, in which case no tuple will be deleted.
 * otherwise only delete tuples with the specified inhparent.
 * 
 * Returns whether at least one row was deleted.
 */
bool 
DeleteMaxCompIndexPointer(Oid relid)
{
    bool deleted = false;
    Relation catalogRelation;
    ScanKeyData key;
    SysScanDesc scan;
    HeapTuple	MaxCompIndexPointerTuple;

    /**
     * Find pg_max_comp_index_pointer entries by relid.
    */

    if (!OidIsValid(relid))
        ereport(ERROR,
                (errcode(ERRCODE_UNDEFINED_TABLE),
                errmsg("relation does not exist")));
    catalogRelation = table_open(MaxCompIndexPointerRelationId,RowExclusiveLock);
    ScanKeyInit(&key,
                Anum_pg_max_comp_index_pointer_relid,
                BTEqualStrategyNumber, F_OIDEQ,
                ObjectIdGetDatum(relid));
    scan = systable_beginscan(catalogRelation,MaxCompIndexPointerSourceLocationIndexId,
                                true, NULL, 1, &key);

	while (HeapTupleIsValid(MaxCompIndexPointerTuple = systable_getnext(scan)))
    {
        Form_pg_max_comp_index_pointer compForm;
        Oid cur_relid;
        compForm = (Form_pg_max_comp_index_pointer) GETSTRUCT(MaxCompIndexPointerTuple);
        cur_relid = compForm->relid;
        if(cur_relid == relid){
			CatalogTupleDelete(catalogRelation, &MaxCompIndexPointerTuple->t_self);
            deleted = true;
        }
    }

    /** Done*/
    systable_endscan(scan);
	table_close(catalogRelation, RowExclusiveLock);

    return deleted;
}