/*-------------------------------------------------------------------------
 * Yuxin Tang
 * 2023.12.1 
 * 
 * pg_compression_index.h
 *	  definition of the compression index table
 *
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California

 * src/include/catalog/pg_compression_index.h
 *
 * NOTES
 *	  The Catalog.pm module reads this file and derives schema
 *	  information.
 *
 *-------------------------------------------------------------------------
 */


#ifndef PG_COMPRESSION_INDEX_H
#define PG_COMPRESSION_INDEX_H
 
#include "catalog/genbki.h"
 
/* ----------------
 *		pg_count definition.  cpp turns this into
 *		typedef struct FormData_pg_count
 * ----------------
 */
#define CompressionIndexRelationId	111
 
CATALOG(pg_compression_index,111,CompressionIndexRelationId) 
{
    /* oid */
    Oid     relid;

    /* rowid */
	int32   rowid;
 
    /* compressed offset */
	int32		compress_off;

    /* uncompressed offset */
	int32		uncompress_off;		
 
} FormData_pg_compression_index;
 

typedef struct index_pair{
    int32 compress_offset;
    int32 uncompress_offset;
}index_pair;

#define Natts_pg_compression_index	4
#define Anum_pg_compression_index_relid	1
#define Anum_pg_compression_index_rowid	2
#define Anum_pg_compression_index_compress_off 3
#define Anum_pg_compression_index_uncompress_off 4
 
/* ----------------
 *		Form_pg_compression_index corresponds to a pointer to a tuple with
 *		the format of pg_compression_index relation.
 * ----------------
 */
typedef FormData_pg_compression_index *Form_pg_compression_index;
 
extern bool HasCompressionIndex(Oid relid, int32 rowid);
extern void InsertCompressionIndex(Oid relid, int32 rowid,
                                int32 compress_off, int32 uncompress_off);
extern void UpdateCompressionIndex(Oid relid, int32 rowid,
                                int32 compress_off, int32 uncompress_off);
extern bool DeleteCompressionIndex(Oid relid, int32 rowid);
extern index_pair searchCompressionIndex(Oid relid,int32 rowid,int32 uncompress_off);
DECLARE_UNIQUE_INDEX(pg_compression_index_source_location_index, 3813, CompressionIndexSourceLocationIndexId, on pg_compression_index using btree(relid oid_ops, rowid int4_ops ,compress_off int4_ops));
#define CompressionIndexSourceLocationIndexId 3813
// DECLARE_UNIQUE_INDEX(pg_compression_index_source_table_index, 3814, CompressionIndexSourceTableIndexId, on pg_compression_index using btree(relid oid_ops,rowid int4_ops));
// #define CompressionIndexSourceTableIndexId 3814
#endif			/* PG_COMPRESSION_INDEX_H */


