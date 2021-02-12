#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "vsimplefs.h"

#define FAT_BLOCK_NO 256
#define FAT_ENTRY_SIZE 8
int FAT_SIZE = BLOCKSIZE / FAT_ENTRY_SIZE * FAT_BLOCK_NO;

#define ROOT_DIR_ENTRY_SIZE 256
#define ROOT_DIR_BLOCK_NO 7
int ROOT_DIR_SIZE = BLOCKSIZE / ROOT_DIR_ENTRY_SIZE * ROOT_DIR_BLOCK_NO;

typedef struct fatEntry {
    int dirNo;
    int nextBlockNo;
} fat_t;

typedef struct rtDirEntry {
    int availability;
    int firstBlockNo;
    int size;
    char name[64];
} rtdr_t;

// Global Variables =======================================
int vdisk_fd;

int oftCount;               // nuber of open files in open file table (oft)
int oftAvailability[16];    // which numbers in the array are open in oft
int oftModes[16];           // modes of open files in oft
rtdr_t **oftDirs;           // root directory structs of files in oft

// read block k from disk (virtual disk) into buffer block.
int read_block (void *block, int k) {
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = read (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("read error\n");
	return -1;
    }
    return (0);
}

// write block k into the virtual disk.
int write_block (void *block, int k) {
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(vdisk_fd, (off_t) offset, SEEK_SET);
    n = write (vdisk_fd, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
	printf ("write error\n");
	return (-1);
    }
    return 0;
}

// initialize empty FAT into blocks 8-265
int init_FAT() {
    int rootDir = ROOT_DIR_BLOCK_NO + 1;
    int offset;

    fat_t *entry = (fat_t *) malloc( sizeof( fat_t));
    entry->dirNo = -1;
    entry->nextBlockNo = -1;

    void *fat = (void *) entry;

    offset = rootDir * BLOCKSIZE;
    for ( int i = 0; i < FAT_SIZE; i++) {
        lseek(vdisk_fd, (off_t) offset, SEEK_SET);
        write(vdisk_fd, fat, sizeof( fat_t));
        offset = offset + FAT_ENTRY_SIZE;
    }
    free( entry);
    return 0;
}

// initialize empty root directory into blocks 1-7
int init_root_dir() {
    int offset;

    rtdr_t *entry = (rtdr_t *) malloc( sizeof( rtdr_t));
    entry->availability = -1;
    entry->firstBlockNo = -1;
    entry->size = 0;

    void *rootDir = (void *) entry;

    offset = BLOCKSIZE;
    for ( int i = 0; i < ROOT_DIR_SIZE; i++) {
        lseek(vdisk_fd, (off_t) offset, SEEK_SET);
        write(vdisk_fd, rootDir, sizeof( rtdr_t));
        offset = offset + ROOT_DIR_ENTRY_SIZE;
    }
    free( entry);
    return 0;
}

// search root directory entries for entry with fileName
int searchDirEntry( char *fileName) {
    rtdr_t *entry;
    void *rt = (void *) malloc( ROOT_DIR_ENTRY_SIZE);

    int offset = BLOCKSIZE;
    for ( int i = 0; i < ROOT_DIR_SIZE; i++) {
        lseek( vdisk_fd, (off_t) offset, SEEK_SET);
        read( vdisk_fd, rt, ROOT_DIR_ENTRY_SIZE);
        offset = offset + ROOT_DIR_ENTRY_SIZE;
        entry = (rtdr_t *) rt;

        if ( entry->availability >= 0
            && strcmp( entry->name, fileName) == 0) {
            free(entry);
            return i;
        }
    }
    free( entry);
    return -1;
}

// create a new root directory entry with given properties
int createDirEntry( char *fileName, int fileSize, int firstBlock) {
    rtdr_t *entry;
    void *rt = (void *) malloc( ROOT_DIR_ENTRY_SIZE);

    int offset = BLOCKSIZE;
    int i;
    for ( i = 0; i < ROOT_DIR_SIZE; i++) {
        lseek( vdisk_fd, (off_t) offset, SEEK_SET);
        read( vdisk_fd, rt, ROOT_DIR_ENTRY_SIZE);
        entry = (rtdr_t *) rt;

        if ( entry->availability < 0) {
            entry->availability = 1;
            entry->firstBlockNo = firstBlock;
            entry->size = fileSize;
            strcpy( entry->name, fileName);

            lseek( vdisk_fd, (off_t) offset, SEEK_SET);
            write( vdisk_fd, rt, sizeof( rtdr_t));
            break;
        }
        offset = offset + ROOT_DIR_ENTRY_SIZE;
    }
    free( entry);

    if ( i == ROOT_DIR_SIZE)
        return -1;
    return i;
}

// get root directory struct of given index
rtdr_t *getDirEntry( int n) {
    rtdr_t *entry;
    void *rt = (void *) malloc( ROOT_DIR_ENTRY_SIZE);

    int offset = BLOCKSIZE + n * ROOT_DIR_ENTRY_SIZE;

    lseek( vdisk_fd, (off_t) offset, SEEK_SET);
    read( vdisk_fd, rt, ROOT_DIR_ENTRY_SIZE);
    entry = (rtdr_t *) rt;

    return entry;
}

// delete root directory struct of given index
int deleteDirEntry( int n) {
    rtdr_t *entry = (rtdr_t *) malloc( sizeof( rtdr_t));
    entry->availability = -1;
    entry->firstBlockNo = -1;
    entry->size = 0;

    void *rt = (void *) entry;

    int offset = BLOCKSIZE + n * ROOT_DIR_ENTRY_SIZE;

    lseek( vdisk_fd, (off_t) offset, SEEK_SET);
    write( vdisk_fd, rt, sizeof( rtdr_t));

    free( entry);

    return 0;
}

// create FAT entry
int createFatEntry( int blockNo) {
    fat_t *entry;
    void *fat = (void *) malloc( FAT_ENTRY_SIZE);

    int offset = (ROOT_DIR_BLOCK_NO + 1) * BLOCKSIZE;
    int i;
    for ( i = 0; i < FAT_SIZE; i++) {
        lseek( vdisk_fd, (off_t) offset, SEEK_SET);
        read( vdisk_fd, fat, FAT_ENTRY_SIZE);
        entry = (fat_t *) fat;

        if ( entry->dirNo < 0) {
            entry->dirNo = 0;
            entry->nextBlockNo = blockNo;

            lseek( vdisk_fd, (off_t) offset, SEEK_SET);
            write( vdisk_fd, fat, FAT_ENTRY_SIZE);
            break;
        }
        offset = offset + FAT_ENTRY_SIZE;
    }
    free( entry);

    if ( i == FAT_SIZE)
        return -1;
    return i;
}

// append nextBlockNo to given indexed FAT entry
int appendFatEntry( int n, int newBlockNo) {
    fat_t *entry;
    void *fat = (void *) malloc( FAT_ENTRY_SIZE);

    int rootDir = (ROOT_DIR_BLOCK_NO + 1) * BLOCKSIZE;
    int offset = rootDir + n * FAT_ENTRY_SIZE;

    lseek( vdisk_fd, (off_t) offset, SEEK_SET);
    read( vdisk_fd, fat, FAT_ENTRY_SIZE);
    entry = (fat_t *) fat;

    if ( entry->nextBlockNo < 0) {
        entry->nextBlockNo = newBlockNo;

        lseek( vdisk_fd, (off_t) offset, SEEK_SET);
        write( vdisk_fd, fat, sizeof( fat_t));
    }
    else {
        free( entry);
        return -1;
    }
    free( entry);

    return 0;
}

// get struct of given indexed FAT entry
fat_t *getFatEntry( int n) {
    fat_t *entry;
    void *fat = (void *) malloc( FAT_ENTRY_SIZE);

    int rootDir = (ROOT_DIR_BLOCK_NO + 1) * BLOCKSIZE;
    int offset = rootDir + n * FAT_ENTRY_SIZE;

    lseek( vdisk_fd, (off_t) offset, SEEK_SET);
    read( vdisk_fd, fat, FAT_ENTRY_SIZE);
    entry = (fat_t *) fat;

    return entry;
}

// delete struct of given indexed FAT entry and vipe corresponding block
int deleteFatEntry( int n) {
    fat_t *entry = (fat_t *) malloc( sizeof( fat_t));
    entry->dirNo = -1;
    entry->nextBlockNo = -1;

    void *fat = (void *) entry;

    int rootDir = (ROOT_DIR_BLOCK_NO + 1) * BLOCKSIZE;
    int offset = rootDir + n * FAT_ENTRY_SIZE;

    lseek( vdisk_fd, (off_t) offset, SEEK_SET);
    write( vdisk_fd, fat, sizeof( fat_t));

    free( entry);

    // truncate block n
    void *nu = (void *) malloc( BLOCKSIZE);

    write_block( nu, n + FAT_BLOCK_NO + ROOT_DIR_BLOCK_NO + 1);

    free( nu);

    return 0;
}

// delete and cascade struct of given indexed FAT entry
int safeDeleteFatEntry( int n) {
    fat_t *entry;
    entry = getFatEntry( n);

    if ( entry->nextBlockNo != -1) {
        int s = safeDeleteFatEntry( entry->nextBlockNo);
        if ( s == -1)
            return -1;
    }

    deleteFatEntry( n);
    return 0;
}

// create and format the disk with vdiskname
int create_format_vdisk (char *vdiskname, unsigned int m) {
    char command[1000];
    int size;
    int num = 1;
    int count;
    size  = num << m;
    count = size / BLOCKSIZE;

    sprintf (command, "dd if=/dev/zero of=%s bs=%d count=%d",
             vdiskname, BLOCKSIZE, count);
    system (command);

    // open and format the disc
    vdisk_fd = open( vdiskname, O_RDWR);
    init_root_dir();
    init_FAT();

    return (0);
}

// mount the disk with vdiskname
int vsfs_mount( char *vdiskname) {
    // simply open the Linux file vdiskname and in this
    // way make it ready to be used for other operations.
    // vdisk_fd is global; hence other function can use it.
    vdisk_fd = open(vdiskname, O_RDWR);

    for ( int i = 0; i < 16; i++)
        oftAvailability[i] = 0;
    oftCount = 0;
    oftDirs = (rtdr_t **) malloc( 16 * sizeof( rtdr_t *));

    return(0);
}

// unmount currently mounted disc
int vsfs_umount() {
    fsync (vdisk_fd); // copy everything in memory to disk
    close (vdisk_fd);

    for ( int i = 0; i < 16; i++) {
        if ( oftAvailability[i] == 1)
            free( oftDirs[i]);
    }
    free( oftDirs);

    return (0);
}

// create a file in disc with given name
int vsfs_create( char *fileName) {
    int blockNo, rtNo;

    int s = searchDirEntry( fileName);
    if ( s != -1)
        return -1;

    blockNo = createFatEntry( -1);
    if ( blockNo < 0)
        return -1;

    rtNo = createDirEntry( fileName, 0, blockNo);
    if ( rtNo < 0) {
        deleteFatEntry( blockNo);
        return -1;
    }

    void *nu = (void *) malloc( BLOCKSIZE);

    write_block( nu, blockNo + FAT_BLOCK_NO + ROOT_DIR_BLOCK_NO + 1);

    free( nu);
    return 0;
}

// open the file with given name in desired mode
int vsfs_open(char *file, int mode) {
    if ( oftCount == 16 || ( mode != MODE_READ && mode != MODE_APPEND))
        return -1;

    for ( int i = 0; i < 16; i++) {
        if ( oftAvailability[i] == 1)
            if ( strcmp( file, oftDirs[i]->name) == 0)
                return -1;
    }

    int index = 0;
    while ( index < 16) {
        if ( oftAvailability[index] == 0) {
            int s = searchDirEntry( file);
            if ( s < 0)
                return -1;

            oftAvailability[index] = 1;
            oftModes[index] = mode;
            oftDirs[index] = getDirEntry( s);

            oftCount++;
            return index;
        }
        index++;
    }
    return -1;
}

// close the file with given file descriptor
int vsfs_close(int fd){
    if ( oftAvailability[fd] == 0)
        return -1;

    oftAvailability[fd] = 0;
    free( oftDirs[fd]);
    oftCount--;

    return 0;
}

// get size of the file with given file descriptor
int vsfs_getsize(int  fd) {
    if ( oftAvailability[fd] == 0)
        return -1;
    return oftDirs[fd]->size;
}

// read n bytes from the file with given file descriptor
int vsfs_read(int fd, void *buf, int n) {
    char *block;
    int resLen = 0, offset;
    int blockNo;
    int dataStart = (FAT_BLOCK_NO + ROOT_DIR_BLOCK_NO + 1) * BLOCKSIZE;

    if ( oftAvailability[fd] == 0)
        return -1;

    if ( oftModes[fd] != MODE_READ)
        return -1;

    if ( n > oftDirs[fd]->size)
        n = oftDirs[fd]->size;

    blockNo = oftDirs[fd]->firstBlockNo;

    while ( resLen < n) {
        if ( blockNo == -1) {
            break;
        }

        offset = dataStart + blockNo * BLOCKSIZE;
        if ( n - resLen > BLOCKSIZE) {
            block = (char *) malloc( BLOCKSIZE);
            lseek( vdisk_fd, (off_t) offset, SEEK_SET);
            read( vdisk_fd, block, BLOCKSIZE);

            strcat( buf, block);
            free( block);

            fat_t *entry = getFatEntry( blockNo);
            blockNo = entry->nextBlockNo;
            free( entry);

            resLen = resLen + BLOCKSIZE;
        }
        else {
            block = (char *) malloc( n - resLen +1);
            lseek( vdisk_fd, (off_t) offset, SEEK_SET);
            read( vdisk_fd, block,  n - resLen);

            strcat( buf, block);
            free( block);

            resLen = n;
        }
    }

    return resLen;
}

// appent n bytes of buf into the file with given file descriptor
int vsfs_append(int fd, void *buf, int n) {
    int remLen, blockLen, offset;
    int blockNo;
    int dataStart = (FAT_BLOCK_NO + ROOT_DIR_BLOCK_NO + 1) * BLOCKSIZE;
    if ( oftAvailability[fd] == 0)
        return -1;

    if ( oftModes[fd] != MODE_APPEND)
        return -1;

    remLen = n;
    blockLen = oftDirs[fd]->size;

    blockNo = oftDirs[fd]->firstBlockNo;

    while ( remLen > 0) {
        offset = dataStart + blockNo * BLOCKSIZE;
        if ( blockLen > BLOCKSIZE) {
            fat_t *entry = getFatEntry( blockNo);
            blockNo = entry->nextBlockNo;
            free( entry);

            blockLen = blockLen - BLOCKSIZE;
        }
        else {
            if ( remLen > BLOCKSIZE - blockLen) {
                lseek( vdisk_fd, (off_t) offset + blockLen, SEEK_SET);
                write( vdisk_fd, buf + (n - remLen), BLOCKSIZE - blockLen);

                remLen = remLen - ( BLOCKSIZE - blockLen);

                int newBlock = createFatEntry( -1);
                int s = appendFatEntry( blockNo, newBlock);
                if ( s == -1) {
                    break;
                }

                blockNo = newBlock;

                blockLen = 0;
            }
            else {
                lseek( vdisk_fd, (off_t) offset + blockLen, SEEK_SET);
                write( vdisk_fd, buf + (n - remLen), remLen);

                remLen = 0;
            }
        }
    }

    oftDirs[fd]->size = oftDirs[fd]->size + n - remLen;

    void *rt = (void *) oftDirs[fd];
    int s = searchDirEntry( oftDirs[fd]->name);
    if ( s == -1)
        return -1;
    offset = BLOCKSIZE + ROOT_DIR_ENTRY_SIZE * s;

    lseek( vdisk_fd, (off_t) offset, SEEK_SET);
    write( vdisk_fd, rt, sizeof( rtdr_t));

    return n - remLen;
}

// delete the file with given name from the disc
int vsfs_delete(char *fileName) {
    int blockNo, rtNo;
    rtdr_t *rootDir;

    rtNo = searchDirEntry( fileName);
    if ( rtNo < 0)
        return -1;

    rootDir = getDirEntry( rtNo);

    blockNo = rootDir->firstBlockNo;

    int s = safeDeleteFatEntry( blockNo);
    if ( s == -1)
        return -1;

    s = deleteDirEntry( rtNo);
    if ( s == -1)
        return -1;

    return 0;
}
