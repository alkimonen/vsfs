# Simple File System Library Implementation
This project implements a single file simple file system implementation in **C**. Maximum size of the file system can be upto 512MB. Initially, it starts from approximately 1MB and allocates more space as new files are created in the system.


## Installation and Compilation
This project uses *Makefile* and *gcc*. These packages should be installed into the system before compilation. If not installed, install them first. Then navigate to the projects directory to compile the project and execute
```
make
```
This process creates the file system library. This can later be used as desired. Also, the project includes a couple test files for simple tests.


## File System Details
The first block is occupied with superblock information. I’ve left this part of the memory empty so that this file system’s superblock information can be implemented according to the needs of the situation.
Next seven blocks are reserved for root directory entries. I’ve designed my root directory entry in less than 256 bytes. However, it still holds 256 bytes as requested so that there will be no confusion. It just doesn’t use all the space in one entry. First 4 bytes hold the availability information of the root directory (0 if there is a file in that location, -1 otherwise). Next 4 bytes hold the information of first block no of the file. Next 4 bytes hold the size of the file which is 0 at the initialization. Then the next 64 bytes hold the name of the file. The remaining 180 bytes can be used for implementing further functions.
| Availability | First Block No | File Size | Name of File |           ...           |
|:------------:|:--------------:|:---------:|:------------:|:-----------------------:|
|    4 bytes   |     4 bytes    |  4 bytes  |   64 bytes   |        180 bytes        |

After root directory, next 256 blocks will be used for file allocation table. I’ve designed a linked file allocation with indexes. Each FAT entry is 8 bytes. First 4 bytes hold an integer which shows availability of the block (0 if occupied, -1 otherwise). Other 4 bytes hold the next block number of the file (-1 is there is no continuation of file).
| Availability | Next Block No |
|:------------:|:--------------:|
|    4 bytes   |     4 bytes    |

After FAT, file blocks start. When disc is formatted, file blocks are not allocated. When a new file is created, block corresponding to the file is allocated. Similarly when some data is appended into the file, if there is need, new blocks are allocated.

## Interface

**int create_format_vdisk(char \*vdiskname, int m):**
This function will be used to create and format a virtual disk. The virtual disk
will simply be a Linux file of certain size. The name of the Linux file is vdiskname.
The parameter m is used to set the size of the filee. Size will be 2^m bytes. The
function will initialize/create a vsfs file system on the virtual disk (this is high-level
formatting of the disk). On-disk file system structures (like superblock, FAT, etc.)
will be initialized on the virtual disk. If success, 0 will be returned. If error, -1 will be
returned.

**int vsfs_mount(char \*vdiskname):**
This function will be used to mount the file system, i.e., to prepare the file
system to be used. This is a simple operation. Basically, it will open the regular Linux
file (acting as the virtual disk) and obtain an integer file descriptor. Other operations
in the library will use this file descriptor. This descriptor will be a global variable in
the library. If success, 0 will be returned; if error, -1 will be returned.

**int vsfs_umount():**
This function will be used to unmount the file system: flush the cached data
to disk and close the virtual disk (Linux file) file descriptor. It is a simple to
implement function. If success, 0 will be returned, if error, -1 will be returned.

**int vsfs_create(char \*filename):**
With this, an applicaton will create a new file with name filename. Your
library implementation of this function will use an entry in the root directory to
store information about the created file, like its name, size, first data block number,
etc. If success, 0 will be returned. If error, -1 will be returned.

**int vsfs_open(char \*filename, int mode):**
With this function an application will open a file. The name of the file to
open is filename. The mode paramater specifies if the file will be opened in readonly
mode or in append-only mode. We can either read the file or append to it. A
file can not be opened for both reading and appending at the same time. In your
library you should have a open file table, and an entry in that table will be used for
the opened file. The index of that entry can be returned as the return value of this
function. Hence the return value will be a non-negative integer acting as a file
descriptor to be used in subsequent file operations. If error, -1 will be returned.

**int vsfs_getsize(int fd):**
With this an application learns the size of a file whose descriptor is fd. File
must be opened first. Returns the number of data bytes in the file. A file with no data
in it (no content) has size 0. If error, returns -1.

**int vsfs_close(int fd):**
With this function an application will close a file whose descriptor is fd. The
related open file table entry should be marked as free.

**int vsfs_read(int fd, void \*buf, int n):**
With this, an application can read data from a file. fd is the file descriptor. buf
is pointing to a memory area for which space is allocated earlier with malloc (or it
can be a static array). n is the amount of data to read. Upon failure, -1 will be
returned. Otherwise, number of bytes sucessfully read will be returned.

**int vsfs_append(int fd, void \*buf, int n):**
With this, an application can append new data to the file. The parameter fd
is the file descriptor. The parameter buf is pointing to (i.e., is the address of) a static
array holding the data or a dynamically allocated memory space holding the data.
The parameter n is the size of the data to write (append) into the file. If error, -1 will
be returned. Otherwise, the number of bytes successfully appended will be returned.

**int vsfs_delete(char \*filename):**
With this, an application can delete a file. The name of the file to be deleted is
filename. If succesful, 0 will be returned. In case of an error, -1 will be returned.
Assume, a process can open at most 16 files simultaneously. Hence your library
should have an open file table with 16 entries.

## Current Status and Recommendations
There are two important points that I want to advert. First one is, if a disc is not formatted it will work correctly when mounted. Before the first use with file system, disc should be formatted. After this, it can be used over and over again in different sessions.
Second point is about vsfs_read function. Buffer given inside the function should be allocated before the call. Also its size should be enough for the amount of desired read. The function doesn’t wipe the buffer and create a new one. It concatenates wanted amount of bytes from the file into the buffer.
