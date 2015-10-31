#ifndef RFS_HASH_H
#define RFS_HASH_H

/* 
 * hash.h: 
 *		header file for routines to manipulate hash_tble lookup, 
 *		insert and delete.
 * 
 * $Id: hash.h,v 1.1 2002/03/11 20:25:52 ningning Exp $
 *
 * Changes:
 * 		$Log: hash.h,v $
 * 		Revision 1.1  2002/03/11 20:25:52  ningning
 * 		hash function for file handle map completely implemented.
 * 		
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "../common/rfs_defines.h"
#include "rfsu_defines.h"

typedef struct rfsfh rfskey_t;

struct hash_ent_struct {
#ifdef HASH_DOUBLE_SAFE
	struct timeval time;
#endif
	rfskey_t key;
	int entry;
	struct hash_ent_struct * next;
};

typedef struct hash_ent_struct hash_ent_t;

typedef struct {
	int size;
	int used_entry;	/* number of hash table entrie which contains valid data */
	int valid_num;	/* number of valid data including those on the chain */
	int (*hash_func) (rfskey_t * key, int size);	
		/*  it's not C++ class, so we have to get size from outside */
	hash_ent_t * buf;
} hash_tbl_t;
 
inline int hash_func1 (rfskey_t * key, int size);
inline int blk_hash_func (blkkey_t * key, int size);
int hash_tbl_init (hash_tbl_t * hash_tbl);
int hash_tbl_insert (hash_tbl_t * hash_tbl, rfskey_t * key, int entry, int mode);
int hash_tbl_delete (hash_tbl_t * hash_tbl, rfskey_t * key);
int hash_tbl_lookup (hash_tbl_t * hash_tbl, rfskey_t * key);

#ifdef ULFS

struct blk_hash_ent_struct {
#ifdef HASH_DOUBLE_SAFE
	struct timeval time;
#endif
	blkkey_t key;
	int entry;
	struct blk_hash_ent_struct * next;
};

typedef struct blk_hash_ent_struct blk_hash_ent_t;

typedef struct {
	int size;
	int keysize;
	int entsize;
	int quiet_flag;
	int used_entry;	/* number of hash table entrie which contains valid data */
	int valid_num;	/* number of valid data including those on the chain */
	int (*hash_func) (blkkey_t * key, int size);	
		/*  it's not C++ class, so we have to get size from outside */
	blk_hash_ent_t * buf;
} blk_hash_tbl_t;

int blk_hash_tbl_init (blk_hash_tbl_t * hash_tbl);
int blk_hash_tbl_insert (blk_hash_tbl_t * hash_tbl, blkkey_t * key, int entry, int mode);
//int blk_hash_tbl_delete (blk_hash_tbl_t * hash_tbl, blkkey_t * key);
int blk_hash_tbl_lookup (blk_hash_tbl_t * hash_tbl, blkkey_t * key);

#endif

#endif
