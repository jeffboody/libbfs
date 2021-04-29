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

#include <stdlib.h>

#define LOG_TAG "bfs"
#include "../libcc/cc_log.h"
#include "../libcc/cc_memory.h"
#include "../libsqlite3/sqlite3.h"
#include "bfs_util.h"

/***********************************************************
* private                                                  *
***********************************************************/

static void* xMalloc(int size)
{
	return MALLOC((size_t) size);
}

static void xFree(void* ptr)
{
	FREE(ptr);
}

static void* xRealloc(void* ptr, int size)
{
	return REALLOC(ptr, (size_t) size);
}

static int xSize(void* ptr)
{
	return (int) MEMSIZEPTR(ptr);
}

static int xRoundup(int size)
{
	return size;
}

static int xInit(void* priv)
{
	return SQLITE_OK;
}

static void xShutdown(void* priv)
{
	return;
}

/***********************************************************
* public                                                   *
***********************************************************/

int bfs_util_initialize(void)
{
	struct sqlite3_mem_methods xmem =
	{
		.xMalloc   = xMalloc,
		.xFree     = xFree,
		.xRealloc  = xRealloc,
		.xSize     = xSize,
		.xRoundup  = xRoundup,
		.xInit     = xInit,
		.xShutdown = xShutdown,
		.pAppData  = NULL
	};
	sqlite3_config(SQLITE_CONFIG_MALLOC, &xmem);

	if(sqlite3_initialize() != SQLITE_OK)
	{
		LOGE("sqlite3_initialize failed");
		return 0;
	}

	return 1;
}

void bfs_util_shutdown(void)
{
	if(sqlite3_shutdown() != SQLITE_OK)
	{
		LOGW("sqlite3_shutdown failed");
	}
}
