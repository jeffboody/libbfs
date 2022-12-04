Blob File System
================

BFS is a file system that is primarily intended to be used
as a replacement for file I/O but can also be used to
replace databases, zip files and XML files in some cases.
The API is designed to be much simpler than any of these
alternatives and can support two underlying file types
which include named binary blobs and key value pairs. The
file system is implemented as an SQLite database which is
more robust, more portable and higher performance than file
I/O.

The following sections describe the library and command
line tool.

BFS Library
===========

Util
----

Since BFS is implemented as an SQLite database, the user
must explicitly initialize and shutdown the underlying
SQLite library. Initialize must be called before any files
are opened and shutdown must be called after all files are
closed. If you are already using SQLite then these functions
may not be needed.

	int  bfs_util_initialize(void);
	void bfs_util_shutdown(void);

File
----

The BFS file system packages all named binary blobs and key
value pairs into a single file that can be accessed through
a bfs\_file\_t handle.

The bfs\_file\_open() and bfs\_file\_close() functions are
used to open/close a file. BFS files include thread
synchronization which allow for a single writer and
multiple readers. The nth parameter specifies how many
threads may be used to simultaneously read entries from the
file. The mode parameter specifies how the file will be
used. The stream mode is designed to handle a special case
where a a file needs to be initialized with many entries.
Files opened for streaming are write only, internal
threading synchronization is disabled and database
transactions are batched together to optimize write
performance.

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

The bfs\_file\_flush() function may be used when streaming
to manually flush outstanding writes.

	int bfs_file_flush(bfs_file_t* self);

The bfs\_file\_attrList() function may be used to list all
attributes in the file via a callback function. The user
must not call into the BFS library from the callback
function or a deadlock will occur.

	typedef int (*bfs_attr_fn)(void* priv,
	                           const char* key,
	                           const char* val);

	int bfs_file_attrList(bfs_file_t* self,
	                      void* priv,
	                      bfs_attr_fn attr_fn);

The bfs\_file\_attrGet() function may be used to get the
value of an attribute. The tid parameter is the thread ID
which must range from 0 to nth-1. At most size bytes will
be written to val (including the terminating null byte).
The bfs\_file\_attrGet() function will return success (1)
with an empty val if the key does not exist in the file.

	int bfs_file_attrGet(bfs_file_t* self,
	                     int tid,
	                     const char* key,
	                     size_t size,
	                     char* val);

The bfs\_file\_attrSet() function may be used to set the
value of an attribute.

	int bfs_file_attrSet(bfs_file_t* self,
	                     const char* key,
	                     const char* val);

The bfs\_file\_attrClr() function may be used to clear the
value of an attribute.

	int bfs_file_attrClr(bfs_file_t* self,
	                     const char* key);

The bfs\_file\_blobList() function may be used to list all
blobs in the file via a callback function. The user
must not call into the BFS library from the callback
function or a deadlock will occur.

	typedef int (*bfs_blob_fn)(void* priv,
	                           const char* name,
	                           size_t size);

	int bfs_file_blobList(bfs_file_t* self,
	                      void* priv,
	                      bfs_blob_fn blob_fn);

The bfs\_file\_blobGet() function may be used to get the
value of a blob. The tid parameter is the thread ID which
must range from 0 to nth-1. The bfs\_file\_blobGet()
function will return success (1) with an empty blob if the
name does not exist in the file. Memory returned by the
bfs\_file\_blobGet() function must be freed by the libcc
FREE() function. The _data parameter may be used in
multiple requests and may be resized accordingly. Note that
the _size parameter will contain the size of the blob which
may not match MEMSIZEPTR(*_data) which contains the size of
the underlying allocation.

	int bfs_file_blobGet(bfs_file_t* self,
	                     int tid,
	                     const char* name,
	                     size_t* _size,
	                     void** _data);

The bfs\_file\_blobSet() function may be used to set the
value of a blob.

	int bfs_file_blobSet(bfs_file_t* self,
	                     const char* name,
	                     size_t size,
	                     const void* data);

The bfs\_file\_blobClr() function may be used to clear the
value of a blob.

	int bfs_file_blobClr(bfs_file_t* self,
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

Blob file paths are typically derived from NAME but can
also be overridden by file paths specified by INPUT/OUTPUT.

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
