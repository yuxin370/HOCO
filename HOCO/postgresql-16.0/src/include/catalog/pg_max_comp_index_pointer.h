/*-------------------------------------------------------------------------
 * Yuxin Tang
 * 2024.1.1
 * 
 * pg_max_comp_index_pointer.h
 *	  definition of the compression index table
 *
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California

 * src/include/catalog/pg_max_comp_index_pointer.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */


#ifndef PG_MAX_COMP_INDEX_POINTER_H
#define PG_MAX_COMP_INDEX_POINTER_H
 
#include "catalog/genbki.h"
 
/* ----------------
 *		pg_count definition.  cpp turns this into
 *		typedef struct FormData_pg_count
 * ----------------
 */
#define MaxCompIndexPointerRelationId	388
 
CATALOG(pg_max_comp_index_pointer,388,MaxCompIndexPointerRelationId) 
{
    /* oid */
    Oid     relid;

    /* maxp */
	int32   maxp;	
 
} FormData_pg_max_comp_index_pointer;
 
 
#define Natts_pg_max_comp_index_pointer	2
#define Anum_pg_max_comp_index_pointer_relid 1
#define Anum_pg_max_comp_index_pointer_maxp	2
 
/* ----------------
 *		Form_pg_max_comp_index_pointer corresponds to a pointer to a tuple with
 *		the format of pg_max_comp_index_pointer relation.
 * ----------------
 */
typedef FormData_pg_max_comp_index_pointer *Form_pg_max_comp_index_pointer;
 
extern bool HasMaxCompIndexPointer(Oid relid);
extern int32 FindMaxCompIndexPointer(Oid relid);
extern void InsertMaxCompIndexPointer(Oid relid, int32 maxp);
extern void UpdateMaxCompIndexPointer(Oid relid, int32 maxp);
extern bool DeleteMaxCompIndexPointer(Oid relid);

DECLARE_UNIQUE_INDEX(pg_max_comp_index_pointer_source_location_index, 3814, MaxCompIndexPointerSourceLocationIndexId, on pg_max_comp_index_pointer using btree(relid oid_ops));
#define MaxCompIndexPointerSourceLocationIndexId 3814
#endif			/* PG_max_comp_index_pointer_H */


