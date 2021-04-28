/*
 * Copyright (c) 2021 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef bfs_file_H
#define bfs_file_H

#include <pthread.h>

#include "../libsqlite3/sqlite3.h"

typedef int (*bfs_attr_fn)(void* priv,
                           const char* key,
                           const char* val);
typedef int (*bfs_blob_fn)(void* priv,
                           const char* name,
                           size_t size);

typedef enum
{
	BFS_MODE_RDONLY = 0,
	BFS_MODE_RDWR   = 1,
	BFS_MODE_STREAM = 2,
} bfs_mode_e;

typedef struct
{
	int        nth;
	bfs_mode_e mode;

	sqlite3* db;

	// sqlite3 statements
	int            batch_size;
	sqlite3_stmt*  stmt_begin;
	sqlite3_stmt*  stmt_end;
	sqlite3_stmt*  stmt_attr_list;
	sqlite3_stmt** stmt_attr_get;
	sqlite3_stmt*  stmt_attr_set;
	sqlite3_stmt*  stmt_attr_clr;
	sqlite3_stmt*  stmt_blob_list;
	sqlite3_stmt** stmt_blob_get;
	sqlite3_stmt*  stmt_blob_set;
	sqlite3_stmt*  stmt_blob_clr;

	// sqlite3 indices
	int idx_attr_get_key;
	int idx_attr_set_key;
	int idx_attr_set_val;
	int idx_attr_clr_key;
	int idx_blob_get_name;
	int idx_blob_set_name;
	int idx_blob_set_blob;
	int idx_blob_clr_name;

	// locking
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	int             readers;
	int             exclusive;
} bfs_file_t;

bfs_file_t* bfs_file_open(const char* fname,
                          int nth,
                          bfs_mode_e mode);
void        bfs_file_close(bfs_file_t** _self);
int         bfs_file_attrList(bfs_file_t* self,
                              void* priv,
                              bfs_attr_fn attr_fn);
int         bfs_file_attrGet(bfs_file_t* self,
                             int tid,
                             const char* key,
                             size_t size,
                             char* val);
int         bfs_file_attrSet(bfs_file_t* self,
                             const char* key,
                             const char* val);
int         bfs_file_attrClr(bfs_file_t* self,
                             const char* key);
int         bfs_file_blobList(bfs_file_t* self,
                              void* priv,
                              bfs_blob_fn blob_fn);
int         bfs_file_blobGet(bfs_file_t* self,
                             int tid,
                             const char* name,
                             size_t* _size,
                             void** _data);
int         bfs_file_blobSet(bfs_file_t* self,
                             const char* name,
                             size_t size,
                             const void* data);
int         bfs_file_blobClr(bfs_file_t* self,
                             const char* name);

#endif
