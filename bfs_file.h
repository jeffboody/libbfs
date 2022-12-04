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

/*
 * callback functions
 */

typedef int (*bfs_attr_fn)(void* priv,
                           const char* key,
                           const char* val);
typedef int (*bfs_blob_fn)(void* priv,
                           const char* name,
                           size_t size);

/*
 * constants
 */

typedef enum
{
	BFS_MODE_RDONLY = 0,
	BFS_MODE_RDWR   = 1,
	BFS_MODE_STREAM = 2,
} bfs_mode_e;

/*
 * opaque objects
 */

typedef struct bfs_file_s bfs_file_t;

/*
 * file API
 */

bfs_file_t* bfs_file_open(const char* fname,
                          int nth,
                          bfs_mode_e mode);
void        bfs_file_close(bfs_file_t** _self);
int         bfs_file_flush(bfs_file_t* self);
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
