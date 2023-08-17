/*
 * Copyright (c) 2022 Jeff Boody
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

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbfs/bfs_file.h"
#include "libbfs/bfs_util.h"

#define LOG_TAG "bfs"
#include "libcc/cc_log.h"
#include "libcc/cc_memory.h"

/***********************************************************
* private                                                  *
***********************************************************/

static void usage(const char* argv0)
{
	ASSERT(argv0);

	LOGE("BFS (Blob File System)");
	LOGE("Usage: %s FILE COMMAND", argv0);
	LOGE("Commands:");
	LOGE("   attrList");
	LOGE("   attrGet KEY");
	LOGE("   attrSet KEY VAL");
	LOGE("   attrClr KEY");
	LOGE("   blobList");
	LOGE("   blobGet NAME [OUTPUT]");
	LOGE("   blobSet NAME [INPUT]");
	LOGE("   blobClr NAME");
}

static int bfs_mkdir(const char* fname)
{
	ASSERT(fname);

	int  len = strnlen(fname, 255);
	char dir[256];
	int  i;
	for(i = 0; i < len; ++i)
	{
		dir[i]     = fname[i];
		dir[i + 1] = '\0';

		if(dir[i] == '/')
		{
			if(access(dir, R_OK) == 0)
			{
				// dir already exists
				continue;
			}

			// try to mkdir
			if(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1)
			{
				if(errno == EEXIST)
				{
					// already exists
				}
				else
				{
					LOGE("mkdir %s failed", dir);
					return 0;
				}
			}
		}
	}

	return 1;
}

static int
bfs_attr_list(void* priv, const char* key, const char* val)
{
	ASSERT(priv);
	ASSERT(key);
	ASSERT(val);

	uint32_t* _count = (uint32_t*) priv;

	if(*_count == 0)
	{
		printf("\n\t\"%s\":\"%s\"", key, val);
	}
	else
	{
		printf(",\n\t\"%s\":\"%s\"", key, val);
	}

	*_count += 1;

	return 1;
}

static int
bfs_blob_list(void* priv, const char* name, size_t size)
{
	ASSERT(priv);
	ASSERT(name);

	size_t* _total = (size_t*) priv;

	printf("%10" PRIu64 " %s\n", (uint64_t) size, name);

	*_total += size;

	return 1;
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, char** argv)
{
	const char* arg0 = argv[0];
	if(argc < 3)
	{
		usage(arg0);
		return EXIT_FAILURE;
	}

	if(bfs_util_initialize() == 0)
	{
		return EXIT_FAILURE;
	}

	bfs_file_t* bfs;
	const char* fname = argv[1];
	const char* cmd   = argv[2];
	if(strcmp(cmd, "attrList") == 0)
	{
		bfs = bfs_file_open(fname, 1, BFS_MODE_RDONLY);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		// output keyval pairs using JSON
		uint32_t count = 0;
		printf("{");
		if(bfs_file_attrList(bfs, (void*) &count,
		                     bfs_attr_list) == 0)
		{
			goto fail_cmd;
		}

		if(count)
		{
			printf("\n");
		}
		printf("}\n");
	}
	else if(strcmp(cmd, "attrGet") == 0)
	{
		if(argc != 4)
		{
			usage(arg0);
			goto fail_shutdown;
		}

		bfs = bfs_file_open(fname, 1, BFS_MODE_RDONLY);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		char* key = argv[3];
		char  val[256];
		if(bfs_file_attrGet(bfs, 0, key, 256, val) == 0)
		{
			goto fail_cmd;
		}

		printf("{\"%s\":\"%s\"}\n", key, val);
	}
	else if(strcmp(cmd, "attrSet") == 0)
	{
		if(argc != 5)
		{
			usage(arg0);
			goto fail_shutdown;
		}

		bfs = bfs_file_open(fname, 1, BFS_MODE_RDWR);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		char* key = argv[3];
		char* val = argv[4];
		if(bfs_file_attrSet(bfs, key, val) == 0)
		{
			goto fail_cmd;
		}
	}
	else if(strcmp(cmd, "attrClr") == 0)
	{
		if(argc != 4)
		{
			usage(arg0);
			goto fail_shutdown;
		}

		bfs = bfs_file_open(fname, 1, BFS_MODE_RDWR);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		char* key = argv[3];
		if(bfs_file_attrClr(bfs, key) == 0)
		{
			goto fail_cmd;
		}
	}
	else if(strcmp(cmd, "blobList") == 0)
	{
		bfs = bfs_file_open(fname, 1, BFS_MODE_RDONLY);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		size_t total = 0;
		if(bfs_file_blobList(bfs, (void*) &total,
		                     bfs_blob_list) == 0)
		{
			goto fail_cmd;
		}
		printf("%10" PRIu64 " bytes\n", (uint64_t) total);
	}
	else if(strcmp(cmd, "blobGet") == 0)
	{
		char* name   = argv[3];
		char* output = NULL;
		if(argc == 5)
		{
			name   = argv[3];
			output = argv[4];
		}
		else if(argc == 4)
		{
			name   = argv[3];
			output = name;
		}
		else
		{
			usage(arg0);
			goto fail_shutdown;
		}

		bfs = bfs_file_open(fname, 1, BFS_MODE_RDONLY);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		size_t size = 0;
		void*  data = NULL;
		if((bfs_file_blobGet(bfs, 0, name, &size, &data) == 0) ||
		   (data == NULL))
		{
			goto fail_cmd;
		}

		if(bfs_mkdir(output) == 0)
		{
			FREE(data);
			goto fail_cmd;
		}

		FILE* f = fopen(output, "w");
		if(f == NULL)
		{
			LOGE("fopen %s failed", output);
			FREE(data);
			goto fail_cmd;
		}

		if(fwrite(data, size, 1, f) != 1)
		{
			LOGE("fwrite failed");
			fclose(f);
			FREE(data);
			goto fail_cmd;
		}

		fclose(f);
		FREE(data);
	}
	else if(strcmp(cmd, "blobSet") == 0)
	{
		char* name  = argv[3];
		char* input = NULL;
		if(argc == 5)
		{
			name  = argv[3];
			input = argv[4];
		}
		else if(argc == 4)
		{
			name  = argv[3];
			input = name;
		}
		else
		{
			usage(arg0);
			goto fail_shutdown;
		}

		bfs = bfs_file_open(fname, 1, BFS_MODE_RDWR);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		FILE* f = fopen(input, "r");
		if(f == NULL)
		{
			LOGE("fopen %s failed", input);
			goto fail_cmd;
		}

		// get file size
		if(fseek(f, (long) 0, SEEK_END) == -1)
		{
			LOGE("fseek failed");
			fclose(f);
			goto fail_cmd;
		}
		size_t size = ftell(f);

		// rewind to start
		if(fseek(f, 0, SEEK_SET) == -1)
		{
			LOGE("fseek failed");
			fclose(f);
			goto fail_cmd;
		}

		// allocate buffer
		void* data = CALLOC(1, size);
		if(data == NULL)
		{
			LOGE("CALLOC failed");
			fclose(f);
			goto fail_cmd;
		}

		// read data
		if(fread(data, size, 1, f) != 1)
		{
			LOGE("fread failed");
			FREE(data);
			fclose(f);
			goto fail_cmd;
		}

		if(bfs_file_blobSet(bfs, name, size, data) == 0)
		{
			FREE(data);
			fclose(f);
			goto fail_cmd;
		}

		FREE(data);
		fclose(f);
	}
	else if(strcmp(cmd, "blobClr") == 0)
	{
		if(argc != 4)
		{
			usage(arg0);
			goto fail_shutdown;
		}

		bfs = bfs_file_open(fname, 1, BFS_MODE_RDWR);
		if(bfs == NULL)
		{
			goto fail_shutdown;
		}

		char* name = argv[3];
		if(bfs_file_blobClr(bfs, name) == 0)
		{
			goto fail_cmd;
		}
	}
	else
	{
		usage(arg0);
		goto fail_shutdown;
	}

	bfs_file_close(&bfs);
	bfs_util_shutdown();

	// success
	return EXIT_SUCCESS;

	// failure
	fail_cmd:
		bfs_file_close(&bfs);
	fail_shutdown:
		bfs_util_shutdown();
	return EXIT_FAILURE;
}
