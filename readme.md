Blob File System
================

BFS simplifies data management by offering a key-value
interface for both binary blobs and attributes. It serves
as a versatile alternative to file I/O, potentially
replacing databases and zip files in specific use cases. The
underlying SQLite database ensures robustness, portability,
and performance exceeding traditional file I/O.

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

C Prototypes:

	int  bfs_util_initialize(void);
	void bfs_util_shutdown(void);

Return Value:

* bfs\_util\_initialize: Returns 1 on success and 0 on error.

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

C Prototypes:

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

Return Value:

* bfs\_file\_open: Returns a bfs\_file\_t handle on success, or
  NULL on error.

Flushing Writes in Streaming Mode
---------------------------------

In streaming mode, bfs\_file\_flush() acts as a checkpoint,
forcing all pending writes to disk. This ensures data
durability in case of unexpected program termination (e.g.
out-of-storage, power failure, or crash).

C Prototype:

	int bfs_file_flush(bfs_file_t* self);

Return Value:

* bfs\_file\_flush: Returns 1 on success, or 0 on error.

Retrieving File Attributes
--------------------------

Use bfs\_file\_attrList() to obtain a list of all
attributes within a file. This function utilizes a callback
function to deliver each attribute to you.

C Prototypes:

	typedef int (*bfs_attr_fn)(void* priv,
	                           const char* key,
	                           const char* val);

	int bfs_file_attrList(bfs_file_t* self,
	                      void* priv,
	                      bfs_attr_fn attr_fn);

Return Value:

* bfs\_attr\_fn: Return 1 to continue attribute enumeration,
  or 0 to stop enumeration and indicate an error.
* bfs\_file\_attrList: Returns 1 on success, or 0 on error.

Important:

* Avoid calling BFS functions from within the callback
  function to prevent deadlocks.

Retriving Attribute Values
--------------------------

Use bfs\_file\_attrGet() to obtain the value of a specific
attribute (key) within a file. Provide a thread ID (tid)
between 0 and n-1 (where n is the number of allowed reader
threads specified at file open) followed by the attribute
key. Up to size bytes of the value, including the null
terminator, will be written to the val buffer you provide.

C Prototype:

	int bfs_file_attrGet(bfs_file_t* self,
	                     int tid,
	                     const char* key,
	                     size_t size,
	                     char* val);

Return Value:

* bfs\_file\_attrGet: Returns 1 on success. If the attribute
  exists, its value is copied to the val buffer. If the
  attribute doesn't exist, val will contain an empty string.
  Returns 0 on error.

Setting Attribute Values
------------------------

Use bfs\_file\_attrSet() to assign a value to a specific
attribute (key) within a file. If an attribute with the
same key already exists, it will be overwritten.

C Prototype:

	int bfs_file_attrSet(bfs_file_t* self,
	                     const char* key,
	                     const char* val);

Return Value:

* bfs\_file\_attrSet: Returns 1 on success, or 0 on error.

Clearing Attribute Values
-------------------------

Use bfs\_file\_attrClr() to remove a specific attribute
(key) from a file.

C Prototype:

	int bfs_file_attrClr(bfs_file_t* self,
	                     const char* key);

Return Value:

* bfs\_file\_attrClr: Returns 1 on success, or 0 on error.

Listing Blobs
-------------

Use bfs\_file\_blobList() to enumerate all blob names within
a file. This function utilizes a callback function to
deliver each blob name to you.

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

C Prototypes:

	typedef int (*bfs_blob_fn)(void* priv,
	                           const char* name,
	                           size_t size);

	int bfs_file_blobList(bfs_file_t* self, void* priv,
	                      bfs_blob_fn blob_fn,
	                      const char* pattern);

Return Value:

* bfs\_blob\_fn: Return 1 to continue blob enumeration,
  or 0 to stop enumeration and indicate an error.
* bfs\_file\_blobList: Returns 1 on success, or 0 on error.

Important:

* Avoid calling BFS functions from within the callback
  function to prevent deadlocks.

Retrieving Blobs
----------------

Use bfs\_file\_blobGet() to obtain the value (data) of a
specific blob within a file. Provide a thread ID (tid)
between 0 and n-1 (where n is the number of allowed reader
threads specified at file open) followed by the blob name.
The function will allocate or reallocate memory for the blob
data and store a pointer to it in data.

C Prototype:

	int bfs_file_blobGet(bfs_file_t* self,
	                     int tid,
	                     const char* name,
	                     size_t* _size,
	                     void** _data);

Return Value:

* bfs\_file\_blobGet: Returns 1 on success. If the blob
  exists, its size will be set and the value is copied to
  the data buffer. If the blob doesn't exist, size be 0.
  Returns 0 on error.

Important:

* The inital call to bfs\_file\_blobGet() should pass NULL
  to the data parameter, however, subsequent calls may
  reuse the returned data parameter.
* The returned data memory must be freed using libcc's
  FREE() function, not the standard C library free().
* The size of the allocated memory for the blob data can be
  obtained using libcc's MEMSIZEPTR() function. This value
  might be larger than the actual blob size returned by
  bfs\_file\_blobGet().

Storing Blobs
-------------

Use bfs\_file\_blobSet() to write a value to a specific blob
(name) within a file. If a blob with the same name already
exists, it will be overwritten.

C Prototype:

	int bfs_file_blobSet(bfs_file_t* self,
	                     const char* name,
	                     size_t size,
	                     const void* data);

Return Value:

* bfs\_file\_blobSet: Returns 1 on success, or 0 on error.

Clearing Blobs
--------------

Use bfs\_file\_blobClr() to clear a specific blob (name)
from a file.

C Prototype:

	int bfs_file_blobClr(bfs_file_t* self,
	                     const char* name);

Return Value:

* bfs\_file\_blobClr: Returns 1 on success, or 0 on error.

BFS Command Line Tool
=====================

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

The BFS library relies on the following external libraries:

* [libcc](https://github.com/jeffboody/libcc): This library
  provides functionalities for logging and memory tracking.
* [libsqlite3](https://github.com/jeffboody/libsqlite3):
  This library provides an interface to the SQLite database
  engine.

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
