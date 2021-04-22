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
typedef int (*bfs_list_fn)(void* priv,
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
	sqlite3_stmt*  stmt_attr;
	sqlite3_stmt** stmt_get;
	sqlite3_stmt*  stmt_set;
	sqlite3_stmt*  stmt_clear;
	sqlite3_stmt*  stmt_list;
	sqlite3_stmt** stmt_load;
	sqlite3_stmt*  stmt_store;
	sqlite3_stmt*  stmt_remove;

	// sqlite3 indices
	int idx_get_key;
	int idx_set_key;
	int idx_set_val;
	int idx_clear_key;
	int idx_load_name;
	int idx_store_name;
	int idx_store_blob;
	int idx_remove_name;

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
int         bfs_file_attr(bfs_file_t* self,
                          void* priv,
                          bfs_attr_fn attr_fn);
int         bfs_file_get(bfs_file_t* self,
                         int tid,
                         const char* key,
                         size_t size,
                         char* val);
int         bfs_file_set(bfs_file_t* self,
                         const char* key,
                         const char* val);
int         bfs_file_list(bfs_file_t* self,
                          void* priv,
                          bfs_list_fn list_fn);
int         bfs_file_load(bfs_file_t* self,
                          int tid,
                          const char* name,
                          size_t* _size,
                          void** _data);
int         bfs_file_store(bfs_file_t* self,
                           const char* name,
                           size_t size,
                           const void* data);

#endif
