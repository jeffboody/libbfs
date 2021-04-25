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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "bfs"
#include "../libcc/cc_log.h"
#include "../libcc/cc_memory.h"
#include "../libsqlite3/sqlite3.h"
#include "bfs_file.h"

#define BATCH_SIZE 10000

/***********************************************************
* private                                                  *
***********************************************************/

static void bfs_file_lockRead(bfs_file_t* self)
{
	ASSERT(self);
	ASSERT(self->mode != BFS_MODE_STREAM);

	pthread_mutex_lock(&self->mutex);
	while(self->exclusive)
	{
		pthread_cond_wait(&self->cond, &self->mutex);
	}
	++self->readers;
	pthread_mutex_unlock(&self->mutex);
}

static void bfs_file_unlockRead(bfs_file_t* self)
{
	ASSERT(self);
	ASSERT(self->mode != BFS_MODE_STREAM);

	pthread_mutex_lock(&self->mutex);
	--self->readers;
	pthread_cond_broadcast(&self->cond);
	pthread_mutex_unlock(&self->mutex);
}

static void bfs_file_lockExclusive(bfs_file_t* self)
{
	ASSERT(self);

	if(self->mode == BFS_MODE_STREAM)
	{
		// ignore
		return;
	}

	pthread_mutex_lock(&self->mutex);
	++self->exclusive;
	while(self->readers)
	{
		pthread_cond_wait(&self->cond, &self->mutex);
	}
}

static void bfs_file_unlockExclusive(bfs_file_t* self)
{
	ASSERT(self);

	if(self->mode == BFS_MODE_STREAM)
	{
		// ignore
		return;
	}

	--self->exclusive;
	pthread_cond_broadcast(&self->cond);
	pthread_mutex_unlock(&self->mutex);
}

static int
bfs_file_initialize(bfs_file_t* self)
{
	ASSERT(self);

	const char* sql_init[] =
	{
		"PRAGMA temp_store_directory = '.';",
		"CREATE TABLE tbl_attr"
		"("
		"   key TEXT NOT NULL,"
		"   val TEXT"
		");",
		"CREATE UNIQUE INDEX idx_attr_key"
		"   ON tbl_attr (key);",
		"CREATE TABLE tbl_blob"
		"("
		"   name TEXT NOT NULL,"
		"   blob BLOB"
		");",
		"CREATE UNIQUE INDEX idx_blob_name"
		"   ON tbl_blob (name);",
		NULL
	};

	// init sqlite3
	int i = 0;
	while(sql_init[i])
	{
		if(sqlite3_exec(self->db, sql_init[i], NULL, NULL,
		                NULL) != SQLITE_OK)
		{
			LOGE("sqlite3_exec(%i): %s",
			     i, sqlite3_errmsg(self->db));
			return 0;
		}
		++i;
	}

	return 1;
}

static int
bfs_file_endTransaction(bfs_file_t* self)
{
	ASSERT(self);

	if((self->mode != BFS_MODE_STREAM) ||
	   (self->batch_size == 0))
	{
		return 1;
	}

	sqlite3_stmt* stmt = self->stmt_end;
	if(sqlite3_step(stmt) == SQLITE_DONE)
	{
		self->batch_size = 0;
	}
	else
	{
		LOGE("sqlite3_step: %s", sqlite3_errmsg(self->db));
		goto fail_step;
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	// success
	return 1;

	// failure
	fail_step:
	{
		if(sqlite3_reset(stmt) != SQLITE_OK)
		{
			LOGW("sqlite3_reset failed");
		}
	}
	return 0;
}

static int
bfs_file_beginTransaction(bfs_file_t* self)
{
	ASSERT(self);

	if(self->mode != BFS_MODE_STREAM)
	{
		return 1;
	}
	else if(self->batch_size >= BATCH_SIZE)
	{
		if(bfs_file_endTransaction(self) == 0)
		{
			return 0;
		}
	}
	else if(self->batch_size > 0)
	{
		++self->batch_size;
		return 1;
	}

	sqlite3_stmt* stmt = self->stmt_begin;
	if(sqlite3_step(stmt) == SQLITE_DONE)
	{
		++self->batch_size;
	}
	else
	{
		LOGE("sqlite3_step: %s", sqlite3_errmsg(self->db));
		goto fail_step;
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	// success
	return 1;

	// failure
	fail_step:
	{
		if(sqlite3_reset(stmt) != SQLITE_OK)
		{
			LOGW("sqlite3_reset failed");
		}
	}
	return 0;
}

static int bfs_fileExists(const char* fname)
{
	ASSERT(fname);

	if(access(fname, F_OK) != 0)
	{
		return 0;
	}
	return 1;
}

/***********************************************************
* public                                                   *
***********************************************************/

bfs_file_t*
bfs_file_open(const char* fname, int nth, bfs_mode_e mode)
{
	ASSERT(fname);

	int flags  = SQLITE_OPEN_READWRITE;
	int exists = bfs_fileExists(fname);
	if(mode == BFS_MODE_RDONLY)
	{
		// database must exist in read-only mode
		if(exists == 0)
		{
			LOGE("invalid %s", fname);
			return NULL;
		}

		flags = SQLITE_OPEN_READONLY;
	}
	else if(mode == BFS_MODE_STREAM)
	{
		if(nth != 1)
		{
			LOGE("invalid nth=%i", nth);
			return NULL;
		}
	}

	// create database if needed
	if(exists == 0)
	{
		flags |= SQLITE_OPEN_CREATE;
	}

	bfs_file_t* self;
	self = (bfs_file_t*)
	       CALLOC(1, sizeof(bfs_file_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->nth  = nth;
	self->mode = mode;

	// sqlite3 must be initialized externally
	if(sqlite3_open_v2(fname, &self->db, flags,
	                   NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_open_v2 %s failed", fname);
		goto fail_db_open;
	}

	if(flags & SQLITE_OPEN_CREATE)
	{
		if(bfs_file_initialize(self) == 0)
		{
			goto fail_initialize;
		}
	}

	const char* sql_begin = "BEGIN;";
	if(sqlite3_prepare_v2(self->db, sql_begin, -1,
	                      &self->stmt_begin,
	                      NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_begin;
	}

	const char* sql_end = "END;";
	if(sqlite3_prepare_v2(self->db, sql_end, -1,
	                      &self->stmt_end,
	                      NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_end;
	}

	const char* sql_attr;
	sql_attr = "SELECT key, val FROM tbl_attr"
	           "   WHERE key=@arg_key;";
	if(sqlite3_prepare_v2(self->db, sql_attr, -1,
	                      &self->stmt_attr,
	                      NULL) != SQLITE_OK)
	if(self->stmt_attr == NULL)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_attr;
	}

	self->stmt_get = (sqlite3_stmt**)
	                 CALLOC(nth, sizeof(sqlite3_stmt*));
	if(self->stmt_get == NULL)
	{
		LOGE("CALLOC failed");
		goto fail_alloc_get;
	}

	int i;
	for(i = 0; i < nth; ++i)
	{
		const char* sql_get;
		sql_get = "SELECT val FROM tbl_attr"
		          "   WHERE key=@arg_key;";
		if(sqlite3_prepare_v2(self->db, sql_get, -1,
		                      &self->stmt_get[i],
		                      NULL) != SQLITE_OK)
		{
			LOGE("sqlite3_prepare_v2: %s",
			     sqlite3_errmsg(self->db));
			goto fail_prepare_get;
		}
	}

	const char* sql_set = "REPLACE INTO tbl_attr (key, val)"
	                      "   VALUES (@arg_key, @arg_val);";
	if(sqlite3_prepare_v2(self->db, sql_set, -1,
	                      &self->stmt_set,
	                      NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_set;
	}

	const char* sql_clear;
	sql_clear = "DELETE FROM tbl_attr"
	            "   WHERE key=@arg_key;";
	if(sqlite3_prepare_v2(self->db, sql_clear, -1,
	                      &self->stmt_clear,
	                      NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_clear;
	}

	const char* sql_list;
	sql_list = "SELECT name, length(blob) FROM tbl_blob;";
	if(sqlite3_prepare_v2(self->db, sql_list, -1,
	                      &self->stmt_list,
	                      NULL) != SQLITE_OK)
	if(self->stmt_list == NULL)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_list;
	}

	self->stmt_load = (sqlite3_stmt**)
	                  CALLOC(nth, sizeof(sqlite3_stmt*));
	if(self->stmt_load == NULL)
	{
		LOGE("CALLOC failed");
		goto fail_alloc_load;
	}

	int j;
	for(j = 0; j < nth; ++j)
	{
		const char* sql_load;
		sql_load = "SELECT blob FROM tbl_blob"
		           "   WHERE name=@arg_name;";
		if(sqlite3_prepare_v2(self->db, sql_load, -1,
		                      &self->stmt_load[j],
		                      NULL) != SQLITE_OK)
		{
			LOGE("sqlite3_prepare_v2: %s",
			     sqlite3_errmsg(self->db));
			goto fail_prepare_load;
		}
	}

	const char* sql_store = "REPLACE INTO tbl_blob (name, blob)"
	                        "   VALUES (@arg_name, @arg_blob);";
	if(sqlite3_prepare_v2(self->db, sql_store, -1,
	                      &self->stmt_store,
	                      NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_store;
	}

	const char* sql_remove;
	sql_remove = "DELETE FROM tbl_blob"
	            "   WHERE name=@arg_name;";
	if(sqlite3_prepare_v2(self->db, sql_remove, -1,
	                      &self->stmt_remove,
	                      NULL) != SQLITE_OK)
	{
		LOGE("sqlite3_prepare_v2: %s",
		     sqlite3_errmsg(self->db));
		goto fail_prepare_remove;
	}

	self->idx_get_key     = sqlite3_bind_parameter_index(self->stmt_get[0],
	                                                     "@arg_key");
	self->idx_set_key     = sqlite3_bind_parameter_index(self->stmt_set,
	                                                     "@arg_key");
	self->idx_set_val     = sqlite3_bind_parameter_index(self->stmt_set,
	                                                    "@arg_val");
	self->idx_clear_key   = sqlite3_bind_parameter_index(self->stmt_clear,
	                                                    "@arg_key");
	self->idx_load_name   = sqlite3_bind_parameter_index(self->stmt_load[0],
	                                                     "@arg_name");
	self->idx_store_name  = sqlite3_bind_parameter_index(self->stmt_store,
	                                                     "@arg_name");
	self->idx_store_blob  = sqlite3_bind_parameter_index(self->stmt_store,
	                                                    "@arg_blob");
	self->idx_remove_name = sqlite3_bind_parameter_index(self->stmt_remove,
	                                                    "@arg_name");

	if(pthread_mutex_init(&self->mutex, NULL) != 0)
	{
		LOGE("pthread_mutex_init failed");
		goto fail_mutex;
	}

	if(pthread_cond_init(&self->cond, NULL) != 0)
	{
		LOGE("pthread_cond_init failed");
		goto fail_cond;
	}

	// success
	return self;

	// failure
	int t;
	fail_cond:
		pthread_mutex_destroy(&self->mutex);
	fail_mutex:
		sqlite3_finalize(self->stmt_remove);
	fail_prepare_remove:
		sqlite3_finalize(self->stmt_store);
	fail_prepare_store:
	fail_prepare_load:
	{
		for(t = 0; t < j; ++t)
		{
			sqlite3_finalize(self->stmt_load[t]);
		}
		FREE(self->stmt_load);
	}
	fail_alloc_load:
		sqlite3_finalize(self->stmt_list);
	fail_prepare_list:
		sqlite3_finalize(self->stmt_clear);
	fail_prepare_clear:
		sqlite3_finalize(self->stmt_set);
	fail_prepare_set:
	fail_prepare_get:
	{
		for(t = 0; t < i; ++t)
		{
			sqlite3_finalize(self->stmt_get[t]);
		}
		FREE(self->stmt_get);
	}
	fail_alloc_get:
		sqlite3_finalize(self->stmt_attr);
	fail_prepare_attr:
		sqlite3_finalize(self->stmt_end);
	fail_prepare_end:
		sqlite3_finalize(self->stmt_begin);
	fail_prepare_begin:
	fail_initialize:
	fail_db_open:
	{
		// sqlite3 must be shutdown externally
		// close db even when open fails
		if(sqlite3_close_v2(self->db) != SQLITE_OK)
		{
			LOGW("sqlite3_close_v2 failed");
		}
		FREE(self);
	}
	return NULL;
}

void bfs_file_close(bfs_file_t** _self)
{
	ASSERT(_self);

	bfs_file_t* self = *_self;
	if(self)
	{
		if(bfs_file_endTransaction(self) == 0)
		{
			// ignore
		}

		pthread_cond_destroy(&self->cond);
		pthread_mutex_destroy(&self->mutex);
		sqlite3_finalize(self->stmt_remove);
		sqlite3_finalize(self->stmt_store);

		int i;
		for(i = 0; i < self->nth; ++i)
		{
			sqlite3_finalize(self->stmt_load[i]);
		}
		FREE(self->stmt_load);

		sqlite3_finalize(self->stmt_list);
		sqlite3_finalize(self->stmt_clear);
		sqlite3_finalize(self->stmt_set);

		for(i = 0; i < self->nth; ++i)
		{
			sqlite3_finalize(self->stmt_get[i]);
		}
		FREE(self->stmt_get);

		sqlite3_finalize(self->stmt_attr);
		sqlite3_finalize(self->stmt_end);
		sqlite3_finalize(self->stmt_begin);

		// sqlite3 must be shutdown externally
		if(sqlite3_close_v2(self->db) != SQLITE_OK)
		{
			LOGW("sqlite3_close_v2 failed");
		}
		FREE(self);
	}
}

int bfs_file_attr(bfs_file_t* self, void* priv,
                  bfs_attr_fn attr_fn)
{
	// priv may be NULL
	ASSERT(self);
	ASSERT(attr_fn);

	if(self->mode == BFS_MODE_STREAM)
	{
		LOGE("invalid mode");
		return 0;
	}

	bfs_file_lockExclusive(self);

	int           ret  = 1;
	sqlite3_stmt* stmt = self->stmt_attr;
	while(sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char* key;
		const char* val;
		key = (const char*) sqlite3_column_text(stmt, 0);
		val = (const char*) sqlite3_column_text(stmt, 1);
		ret &= (*attr_fn)(priv, key, val);
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	bfs_file_unlockExclusive(self);

	return ret;
}

int bfs_file_get(bfs_file_t* self, int tid,
                 const char* key,
                 size_t size, char* val)
{
	ASSERT(self);
	ASSERT(key);
	ASSERT(size > 0);
	ASSERT(val);

	if(self->mode == BFS_MODE_STREAM)
	{
		LOGE("invalid mode");
		return 0;
	}

	bfs_file_lockRead(self);

	int           ret  = 0;
	int           idx  = self->idx_get_key;
	sqlite3_stmt* stmt = self->stmt_get[tid];
	if(sqlite3_bind_text(stmt, idx, key, -1,
	                     SQLITE_TRANSIENT) != SQLITE_OK)
	{
		LOGE("sqlite3_bind_text failed");
		bfs_file_unlockRead(self);
		return 0;
	}

	if(sqlite3_step(stmt) == SQLITE_ROW)
	{
		const char* tmp;
		tmp = (const char*) sqlite3_column_text(stmt, 0);
		if(tmp)
		{
			snprintf(val, size, "%s", tmp);
			ret = 1;
		}
	}
	else
	{
		LOGE("sqlite3_step: msg=%s", sqlite3_errmsg(self->db));
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	bfs_file_unlockRead(self);

	return ret;
}

int bfs_file_set(bfs_file_t* self, const char* key,
                 const char* val)
{
	// val may be NULL
	ASSERT(self);
	ASSERT(key);

	bfs_file_lockExclusive(self);
	if(bfs_file_beginTransaction(self) == 0)
	{
		bfs_file_unlockExclusive(self);
		return 0;
	}

	int           idx_key;
	int           idx_val;
	sqlite3_stmt* stmt;
	if(val)
	{
		idx_key = self->idx_set_key;
		idx_val = self->idx_set_val;
		stmt    = self->stmt_set;
		if((sqlite3_bind_text(stmt, idx_key, key, -1,
		                      SQLITE_TRANSIENT) != SQLITE_OK) ||
		   (sqlite3_bind_text(stmt, idx_val, val, -1,
		                      SQLITE_TRANSIENT) != SQLITE_OK))
		{
			LOGE("sqlite3_bind_text failed");
			bfs_file_unlockExclusive(self);
			return 0;
		}
	}
	else
	{
		idx_key = self->idx_set_key;
		stmt    = self->stmt_clear;
		if(sqlite3_bind_text(stmt, idx_key, key, -1,
		                     SQLITE_TRANSIENT) != SQLITE_OK)
		{
			LOGE("sqlite3_bind_text failed");
			bfs_file_unlockExclusive(self);
			return 0;
		}
	}

	int ret = 1;
	if(sqlite3_step(stmt) != SQLITE_DONE)
	{
		LOGE("sqlite3_step: %s", sqlite3_errmsg(self->db));
		ret = 0;
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	bfs_file_unlockExclusive(self);

	return ret;
}

int bfs_file_list(bfs_file_t* self, void* priv,
                  bfs_list_fn list_fn)
{
	// priv may be NULL
	ASSERT(self);
	ASSERT(list_fn);

	if(self->mode == BFS_MODE_STREAM)
	{
		LOGE("invalid mode");
		return 0;
	}

	bfs_file_lockExclusive(self);

	int           ret  = 1;
	sqlite3_stmt* stmt = self->stmt_list;
	while(sqlite3_step(stmt) == SQLITE_ROW)
	{
		size_t      size;
		const char* name;
		name = (const char*) sqlite3_column_text(stmt, 0);
		size = (size_t) sqlite3_column_int(stmt, 1);
		ret &= (*list_fn)(priv, name, size);
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	bfs_file_unlockExclusive(self);

	return ret;
}

int bfs_file_load(bfs_file_t* self, int tid,
                  const char* name,
                  size_t* _size, void** _data)
{
	ASSERT(self);
	ASSERT(name);
	ASSERT(_size);
	ASSERT(_data);
	ASSERT(*_size == 0);
	ASSERT(*_data == NULL);

	if(self->mode == BFS_MODE_STREAM)
	{
		LOGE("invalid mode");
		return 0;
	}

	bfs_file_lockRead(self);

	int           ret  = 0;
	int           idx  = self->idx_load_name;
	sqlite3_stmt* stmt = self->stmt_load[tid];
	if(sqlite3_bind_text(stmt, idx, name, -1,
	                     SQLITE_TRANSIENT) != SQLITE_OK)
	{
		LOGE("sqlite3_bind_text failed");
		bfs_file_unlockRead(self);
		return 0;
	}

	if(sqlite3_step(stmt) == SQLITE_ROW)
	{
		int         size = 0;
		const void* data = NULL;
		size = sqlite3_column_bytes(stmt, 0);
		data = sqlite3_column_blob(stmt, 0);
		if(size && data)
		{
			*_data = CALLOC(1, size);
			if(*_data)
			{
				memcpy(*_data, data, size);
				*_size = size;
				ret = 1;
			}
			else
			{
				LOGE("CALLOC failed");
			}
		}
		else
		{
			LOGE("invalid size=%i, data=%p", size, data);
		}
	}
	else
	{
		LOGE("sqlite3_step: msg=%s", sqlite3_errmsg(self->db));
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	bfs_file_unlockRead(self);

	return ret;
}

int bfs_file_store(bfs_file_t* self, const char* name,
                   size_t size, const void* data)
{
	// data may be NULL
	ASSERT(self);
	ASSERT(name);

	bfs_file_lockExclusive(self);
	if(bfs_file_beginTransaction(self) == 0)
	{
		bfs_file_unlockExclusive(self);
		return 0;
	}

	int           idx_name;
	int           idx_blob;
	sqlite3_stmt* stmt;
	if(data)
	{
		idx_name = self->idx_store_name;
		idx_blob = self->idx_store_blob;
		stmt     = self->stmt_store;
		if((sqlite3_bind_text(stmt, idx_name, name, -1,
		                      SQLITE_TRANSIENT) != SQLITE_OK) ||
		   (sqlite3_bind_blob(stmt, idx_blob,
		                      data, size,
		                      SQLITE_TRANSIENT) != SQLITE_OK))
		{
			LOGE("sqlite3_bind_text/sqlite3_bind_blob failed");
			bfs_file_unlockExclusive(self);
			return 0;
		}
	}
	else
	{
		idx_name = self->idx_remove_name;
		stmt     = self->stmt_remove;
		if(sqlite3_bind_text(stmt, idx_name, name, -1,
		                     SQLITE_TRANSIENT) != SQLITE_OK)
		{
			LOGE("sqlite3_bind_text: %s", sqlite3_errmsg(self->db));
			bfs_file_unlockExclusive(self);
			return 0;
		}
	}

	int ret = 1;
	if(sqlite3_step(stmt) != SQLITE_DONE)
	{
		LOGE("sqlite3_step: %s", sqlite3_errmsg(self->db));
		ret = 0;
	}

	if(sqlite3_reset(stmt) != SQLITE_OK)
	{
		LOGW("sqlite3_reset failed");
	}

	bfs_file_unlockExclusive(self);

	return ret;
}
