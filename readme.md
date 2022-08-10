Blob File System
================

BFS is a file system that supports binary blobs and key
value pairs. The BFS API is much simpler than alternatives
such as zip files or direct FILE I/O. BFS files are
implemented as an SQLite database that provides a very
robust underlying file format that also have better
performance than direct FILE I/O. BFS includes thread
synchronization that allows for a single writer and
multiple readers.

The following sections describe the library and command
line tool.

BFS Library
===========

Return Values
-------------

Integer return values should be interpreted as success (1)
or failure (0).

Util
----

The BFS util API simply includes two functions which are
used to initialize and shutdown SQLite. Initialize must be
called before any files are opened and shutdown must be
called after all files are closed. If you are already using
SQLite then these functions may not be needed.

	int  bfs_util_initialize(void);
	void bfs_util_shutdown(void);

File
----

BFS file API allows the user to interact with key-value
pairs and named binary blobs.

The bfs\_file\_open() and bfs\_file\_close() functions are
used to open/close a file. The nth parameter specifies how
many threads may be used to read entries from the file. The
mode parameter specifies how the file will be used. The
stream mode is designed to handle a special case where a
a file needs to be initialized with many entries. Files
opened for streaming are write only, internal threading
synchronization is disabled and database transactions are
batched together to optimize write performance.

	typedef enum
	{
		BFS_MODE_RDONLY = 0,
		BFS_MODE_RDWR   = 1,
		BFS_MODE_STREAM = 2,
	} bfs_mode_e;

	bfs_file_t* bfs_file_open(const char* fname,
	                          int nth,
	                          bfs_mode_e mode);
	void        bfs_file_close(bfs_file_t** _self);

The bfs\_file\_attrList() function may be used to list all
attributes in the file.

	typedef int (*bfs_attr_fn)(void* priv,
	                           const char* key,
	                           const char* val);

	int         bfs_file_attrList(bfs_file_t* self,
	                              void* priv,
	                              bfs_attr_fn attr_fn);

The bfs\_file\_attrGet(), bfs\_file\_attrSet(), and
bfs\_file\_attrClr() functions may be used to get, set and
clear attributes. The tid parameter is the thread id which
must range from 0 to nth-1. The bfs\_file\_attrGet()
function will return success with an empty val if the key
does not exist in the file.

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

The bfs\_file\_blobList() function may be used to list all
binary blobs in the file.

	typedef int (*bfs_blob_fn)(void* priv,
	                           const char* name,
	                           size_t size);

	int         bfs_file_blobList(bfs_file_t* self,
	                              void* priv,
	                              bfs_blob_fn blob_fn);

The bfs\_file\_blobGet(), bfs\_file\_blobSet(), and
bfs\_file\_blobClr() functions may be used to get, set and
clear binary blobs. The tid parameter is the thread id which
must range from 0 to nth-1. The bfs\_file\_blobGet()
function will return success with an empty blob if the name
does not exist in the file. Memory returned by the
bfs\_file\_blobGet() function must be freed by the libcc
FREE() function.

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

Command Line Tool
=================

Usage
-----

Key/Val Attributes

	bfs FILE attrList
	bfs FILE attrGet KEY
	bfs FILE attrSet KEY VAL
	bfs FILE attrClr KEY

Named Blobs

	bfs FILE blobList
	bfs FILE blobGet NAME [OUTPUT]
	bfs FILE blobSet NAME [INPUT]
	bfs FILE blobClr NAME

Dependencies
============

The BFS library uses
[libcc](https://github.com/jeffboody/libcc)
for logging and memory tracking.

SQLite database support is provided by
[libsqlite3](https://github.com/jeffboody/libsqlite3).

License
=======

The BFS library was developed by
[Jeff Boody](mailto:jeffboody@gmail.com)
under The MIT License.

	Copyright (c) 2021 Jeff Boody

	Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
