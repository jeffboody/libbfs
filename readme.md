Blob File System
================

BFS simplifies data management by offering a key-value
interface for both binary blobs and attributes. It serves
as a versatile alternative to file I/O, potentially
replacing databases and zip files in specific use cases. The
underlying SQLite database ensures robustness, portability,
and performance exceeding traditional file I/O. The
attributes are intended to serve the same role as a
traditional file header.

BFS Library
===========

Initialization and Shutdown
---------------------------

Unlike traditional file systems, BFS relies on SQLite for
data storage. For proper functionality, users must
explicitly initialize the library using
bfs\_util\_initialize() before any file operations.
Similarly, shutdown using bfs\_util\_shutdown() is required
after all files are closed to ensure data integrity and
resource management. If your application already uses and
manages SQLite initialization, these calls might not be
necessary for BFS.

C Prototype

	int  bfs_util_initialize(void);
	void bfs_util_shutdown(void);

Unified File Interface
----------------------

BFS provides a single file interface (bfs\_file\_t) for
working with both binary blobs and attributes within a file.

File Open/Close
---------------

BFS uses bfs\_file\_open() and bfs\_file\_close() to manage
files. It supports thread-safety with one writer and
multiple readers. When opening a file, you specify the
allowed number of reader threads (nth). The mode parameter
controls file usage. The stream mode is a write-only
optimization for initializing files with many entries. It
disables internal threading and uses batched database
transactions to improve write performance.

C Prototype

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

Flushing Writes in Streaming Mode
---------------------------------

In streaming mode, bfs\_file\_flush() acts as a checkpoint,
forcing all pending writes to disk. This ensures data
durability in case of unexpected program termination (e.g.
out-of-storage, power failure, or crash).

C Prototype

	int bfs_file_flush(bfs_file_t* self);

Retrieving File Attributes
--------------------------

Use bfs\_file\_attrList() to obtain a list of all
attributes within a file. This function utilizes a callback
function to deliver each attribute to you. Important: Avoid
calling BFS functions from within the callback function to
prevent deadlocks.

C Prototype

	typedef int (*bfs_attr_fn)(void* priv,
	                           const char* key,
	                           const char* val);

	int bfs_file_attrList(bfs_file_t* self,
	                      void* priv,
	                      bfs_attr_fn attr_fn);

Retriving Attribute Values
--------------------------

Use bfs\_file\_attrGet() to obtain the value of a specific
attribute (key) within a file. Provide a thread ID (tid)
between 0 and n-1 (where n is the number of allowed reader
threads specified at file open) followed by the attribute
key. Up to size bytes of the value, including the null
terminator, will be written to the val buffer you provide.
The function returns 1 (success) even if the key doesn't
exist, in which case val will be empty. If an internal
SQLite error occurs, the function returns 0 (error).

C Prototype

	int bfs_file_attrGet(bfs_file_t* self,
	                     int tid,
	                     const char* key,
	                     size_t size,
	                     char* val);

Setting Attribute Values
------------------------

Use bfs\_file\_attrSet() to assign a value to a specific
attribute (key) within a file.

C Prototype

	int bfs_file_attrSet(bfs_file_t* self,
	                     const char* key,
	                     const char* val);

Clearing Attribute Values
-------------------------

Use bfs\_file\_attrClr() to remove a specific attribute
(key) from a file.

C Prototype

	int bfs_file_attrClr(bfs_file_t* self,
	                     const char* key);

Listing Blobs
-------------

Use bfs\_file\_blobList() to enumerate all blob names within
a file. This function utilizes a callback function to
deliver each blob name to you. Important: Avoid calling BFS
functions from within the callback function to prevent
deadlocks.

Optional Filtering: You can optionally provide a search
pattern to filter the listed blobs based on their names. BFS
supports two wildcard characters:

* %: Matches any sequence of zero or more characters.
* _: Matches a single character.

For example, the pattern "image/%.png" would return all blob
names matching the pattern "image/[any characters].png",
such as "image/photo1.png" or "image/landscape.png". This
can be useful for finding blobs with specific prefixes or
patterns in their names.

C Prototype

	typedef int (*bfs_blob_fn)(void* priv,
	                           const char* name,
	                           size_t size);

	int bfs_file_blobList(bfs_file_t* self, void* priv,
	                      bfs_blob_fn blob_fn,
	                      const char* pattern);

Retrieving Blobs
----------------

Use bfs\_file\_blobGet() to obtain the value (data) of a
specific blob within a file. Provide a thread ID (tid)
between 0 and n-1 (where n is the number of allowed reader
threads specified at file open) followed by the blob name.
The function will allocate or reallocate memory for the blob
data and store a pointer to it in \*\_data. The function
returns the size of the retrieved data. Note:
bfs\_file\_blobGet() is not supported in streaming mode.

* Success: Return value of 1.
* Error: Return value of 0, indicating an internal SQLite
  error, memory allocation failure, or invalid mode.

Important:

* The returned data memory must be freed using libcc's
  FREE() function, not the standard C library free().
* The size of the allocated memory for the blob data can be
  obtained using libcc's MEMSIZEPTR(*_data) function. This
  value might be larger than the actual blob size returned
  by bfs\_file\_blobGet().

C Prototype

	int bfs_file_blobGet(bfs_file_t* self,
	                     int tid,
	                     const char* name,
	                     size_t* _size,
	                     void** _data);

Storing Blobs
-------------

Use bfs\_file\_blobSet() to write a value to a specific blob
(name) within a file. If a blob with the same name already
exists, it will be overwritten.

C Prototype

	int bfs_file_blobSet(bfs_file_t* self,
	                     const char* name,
	                     size_t size,
	                     const void* data);

Clearing Blobs
--------------

Use bfs\_file\_blobClr() to clear a specific blob (name)
from a file.

C Prototype

	int bfs_file_blobClr(bfs_file_t* self,
	                     const char* name);

Command Line Tool
=================

Attributes
----------

List, retrieve, assign and clear attributes.

	bfs FILE attrList
	bfs FILE attrGet KEY
	bfs FILE attrSet KEY VAL
	bfs FILE attrClr KEY

Blobs
-----

List, retrieve, assign and clear blobs.

	bfs FILE blobList [PATTERN]
	bfs FILE blobGet NAME [OUTPUT]
	bfs FILE blobSet NAME [INPUT]
	bfs FILE blobClr NAME

* PATTERN: An optional search pattern to filter blobs based
  on their names. Supports wildcard characters % (matches
  any sequence of characters) and \_ (matches any single
  character).
* OUTPUT: An optional file path to store the blob in binary
  format.
* INPUT: An optional file path to retrieve the blob in
  binary format.

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
