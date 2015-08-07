// os345fat.c - file management system
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************
//
//		11/19/2011	moved getNextDirEntry to P6
//
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345fat.h"

// ***********************************************************************
// ***********************************************************************
//	functions to implement in Project 6
//
int fmsCloseFile(int);
int fmsDefineFile(char*, int);
int fmsDeleteFile(char*);
int fmsOpenFile(char*, int);
int fmsReadFile(int, char*, int);
int fmsSeekFile(int, int);
int fmsWriteFile(int, char*, int);
int fmsGetFDEntry(DirEntry* dirEntry);
int getFreeCluster();
int fmsUpdateDirEntry(FDEntry* fdEntry);

// ***********************************************************************
// ***********************************************************************
//	Support functions available in os345p6.c
//
extern int fmsGetDirEntry(char* fileName, DirEntry* dirEntry);
extern int fmsGetNextDirEntry(int *dirNum, char* mask, DirEntry* dirEntry, int dir);

extern int fmsMount(char* fileName, void* ramDisk);

extern void setFatEntry(int FATindex, unsigned short FAT12ClusEntryVal, unsigned char* FAT);
extern unsigned short getFatEntry(int FATindex, unsigned char* FATtable);

extern int fmsMask(char* mask, char* name, char* ext);
extern void setDirTimeDate(DirEntry* dir);
extern int isValidFileName(char* fileName);
extern void printDirectoryEntry(DirEntry*);
extern void fmsError(int);

extern int fmsReadSector(void* buffer, int sectorNumber);
extern int fmsWriteSector(void* buffer, int sectorNumber);

// ***********************************************************************
// ***********************************************************************
// fms variables
//
// RAM disk
unsigned char RAMDisk[SECTORS_PER_DISK * BYTES_PER_SECTOR];

// File Allocation Tables (FAT1 & FAT2)
unsigned char FAT1[NUM_FAT_SECTORS * BYTES_PER_SECTOR];
unsigned char FAT2[NUM_FAT_SECTORS * BYTES_PER_SECTOR];

char dirPath[128];							// current directory path
FDEntry OFTable[NFILES];					// open file table

extern bool diskMounted;					// disk has been mounted
extern TCB tcb[];							// task control block
extern int curTask;							// current task #


// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
//    printf("\nBegin Close File");
    int error, index, sectorNumber;
    FDEntry* fdEntry;
    fdEntry = &OFTable[fileDescriptor];
    char buffer[BYTES_PER_SECTOR];

    if (fdEntry->name[0] == 0) return ERR63; // file not open

    if (fdEntry->mode != OPEN_READ) { // file was potentially altered and dirInfo needs to be updated
//        printf("\nFile was potentially updated");

        if (fdEntry->flags & BUFFER_ALTERED) { // buffer needs to be written back as it has been changed
            if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) {
                return error;
            }
            fdEntry->flags ^= BUFFER_ALTERED;
//            printf("\nWrote unflushed buffer");
        }

        if ((error = fmsUpdateDirEntry(fdEntry))) {
            printf("\nErrored in update");
            return error;
        }

//        printf("\nUpdated file information");
    }

    fdEntry->name[0] = 0;
//    printf("\nReleased openFileDescriptor");
	return 0;
} // end fmsCloseFile



// ***********************************************************************
// ***********************************************************************
// If attribute=DIRECTORY, this function creates a new directory
// file directoryName in the current directory.
// The directory entries "." and ".." are also defined.
// It is an error to try and create a directory that already exists.
//
// else, this function creates a new file fileName in the current directory.
// It is an error to try and create a file that already exists.
// The start cluster field should be initialized to cluster 0.  In FAT-12,
// files of size 0 should point to cluster 0 (otherwise chkdsk should report an error).
// Remember to change the start cluster field from 0 to a free cluster when writing to the
// file.
//
// Return 0 for success, otherwise, return the error number.
//
int fmsDefineFile(char* fileName, int attribute)
{
	// ?? add code here
	printf("\nfmsDefineFile Not Implemented");

	return ERR72;
} // end fmsDefineFile



// ***********************************************************************
// ***********************************************************************
// This function deletes the file fileName from the current director.
// The file name should be marked with an "E5" as the first character and the chained
// clusters in FAT 1 reallocated (cleared to 0).
// Return 0 for success; otherwise, return the error number.
//
int fmsDeleteFile(char* fileName)
{
	// ?? add code here
	printf("\nfmsDeleteFile Not Implemented");

	return ERR61;
} // end fmsDeleteFile



// ***********************************************************************
// ***********************************************************************
// This function opens the file fileName for access as specified by rwMode.
// It is an error to try to open a file that does not exist.
// The open mode rwMode is defined as follows:
//    0 - Read access only.
//       The file pointer is initialized to the beginning of the file.
//       Writing to this file is not allowed.
//    1 - Write access only.
//       The file pointer is initialized to the beginning of the file.
//       Reading from this file is not allowed.
//    2 - Append access.
//       The file pointer is moved to the end of the file.
//       Reading from this file is not allowed.
//    3 - Read/Write access.
//       The file pointer is initialized to the beginning of the file.
//       Both read and writing to the file is allowed.
// A maximum of 32 files may be open at any one time.
// If successful, return a file descriptor that is used in calling subsequent file
// handling functions; otherwise, return the error number.
//
int fmsOpenFile(char* fileName, int rwMode)
{
    int fdIndex;
    DirEntry dirEntry;

    if (fmsGetDirEntry(fileName,&dirEntry)) {
        return ERR61; // file was not found
    }

    if (dirEntry.attributes & DIRECTORY) {
        return ERR50; //invalid file name
    }

    switch (rwMode) {
        case OPEN_READ:
            break;
        case OPEN_WRITE:
        case OPEN_APPEND:
        case OPEN_RDWR:
            if (!(dirEntry.attributes & READ_ONLY)) { // checks that file is not read only
                break;
            }
        default:
            return ERR85; //Illegal access
    }

    if ((fdIndex = fmsGetFDEntry(&dirEntry)) < 0) {
        //error finding fdEntry return that error
        return fdIndex;
    }

    //prepare the File Descriptor entry
    FDEntry* fdEntry = &OFTable[fdIndex];
    strncpy(fdEntry->name,dirEntry.name, 8);
    strncpy(fdEntry->extension,dirEntry.extension, 3);
    fdEntry->attributes = dirEntry.attributes;
    fdEntry->directoryCluster = (uint16) CDIR;
    fdEntry->startCluster = dirEntry.startCluster;
    fdEntry->currentCluster = 0;
    fdEntry->fileSize = (rwMode == OPEN_WRITE ? 0 : dirEntry.fileSize);
    fdEntry->pid = curTask;
    fdEntry->mode = rwMode;
    fdEntry->flags = 0x00;
    fdEntry->fileIndex = (rwMode != OPEN_APPEND ? 0 : dirEntry.fileSize);

    // If file is being appended to
    if (rwMode == OPEN_APPEND) {
        unsigned short nextCluster;
        int error;
        fdEntry->currentCluster = fdEntry->startCluster;
        // fast-forward currentCluster to the end of the file
        while ((nextCluster = getFatEntry(fdEntry->currentCluster,FAT1)) != FAT_EOC)
            fdEntry->currentCluster = nextCluster;
        // load the end of the file into the buffer
        if ((error = fmsReadSector(fdEntry->buffer,C_2_S(fdEntry->currentCluster)))) return error;
    }

    return fdIndex;
} // end fmsOpenFile

// ***********************************************************************
// ***********************************************************************
// This function searches the OFTable to find a empty slot for the given file
// returns 0 if successful
// error if:
// - if another file is found with the same name and ext
// - Too many files open
int fmsGetFDEntry(DirEntry* dirEntry)
{
    int i, entryInd = -1;
    FDEntry* fdEntry;

    for (i = 0; i < 32; i++) {
        fdEntry = &OFTable[i];
        if (!strncmp(fdEntry->name, dirEntry->name, 8) && !strncmp(fdEntry->extension, dirEntry->extension, 3)) {
            //both the name and extension match
            return ERR62;
        }

        if (entryInd == -1 && fdEntry->name[0] == 0) {
            entryInd = i;
        }
    }

    return entryInd > -1 ? entryInd : ERR70; //if we found a entry return it otherwise too many files are open
}

// ***********************************************************************
// ***********************************************************************
// This function reads nBytes bytes from the open file specified by fileDescriptor into
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// After each read, the file pointer is advanced.
// Return the number of bytes successfully read (if > 0) or return an error number.
// (If you are already at the end of the file, return EOF error.  ie. you should never
// return a 0.)
//
int fmsReadFile(int fileDescriptor, char* buffer, int nBytes)
{
    int error; uint16 nextCluster;
    FDEntry* fdEntry;
    int numBytesRead = 0;
    unsigned int bytestLeft, bufferIndex;
    fdEntry = &OFTable[fileDescriptor];
    if (fdEntry->name[0] == 0) return ERR63; // file not open
    if ((fdEntry->mode == OPEN_WRITE) || (fdEntry->mode == OPEN_APPEND)) return ERR85;

    while (nBytes > 0) {
        if (fdEntry->fileSize == fdEntry->fileIndex) {
            return (numBytesRead ? numBytesRead : ERR66); //number of bytes read or EOF error
        }
        bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;
        if ((bufferIndex == 0) && (fdEntry->fileIndex || !fdEntry->currentCluster)) {
            if (fdEntry->currentCluster == 0) { //file has been lazy opened and buffer is not initialized
                if (fdEntry->startCluster == 0) {
                    return ERR66;
                }
                nextCluster = fdEntry->startCluster;
                fdEntry->fileIndex = 0;
            }
            else {
                nextCluster = getFatEntry(fdEntry->currentCluster,FAT1);
                if (nextCluster == FAT_EOC) {
                    return numBytesRead;
                }
            }
            if (fdEntry->flags & BUFFER_ALTERED) {
                if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) {
                    return error;
                }
                fdEntry->flags &= ~BUFFER_ALTERED;
            }
            if ((error = fmsReadSector(fdEntry->buffer, C_2_S(nextCluster)))) {
                return error;
            }
            fdEntry->currentCluster = nextCluster;
        }
        bytestLeft = BYTES_PER_SECTOR - bufferIndex;
        if (bytestLeft > nBytes) {
            bytestLeft = nBytes;
        }
        if (bytestLeft > (fdEntry->fileSize - fdEntry->fileIndex)) {
            bytestLeft = fdEntry->fileSize - fdEntry->fileIndex;
        }
        memcpy(buffer, &fdEntry->buffer[bufferIndex],bytestLeft);
        fdEntry->fileIndex += bytestLeft;
        numBytesRead += bytestLeft;
        buffer += bytestLeft;
        nBytes -= bytestLeft;
    }

	return numBytesRead;
} // end fmsReadFile



// ***********************************************************************
// ***********************************************************************
// This function changes the current file pointer of the open file specified by
// fileDescriptor to the new file position specified by index.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// The file position may not be positioned beyond the end of the file.
// Return the new position in the file if successful; otherwise, return the error number.
//
int fmsSeekFile(int fileDescriptor, int index)
{
	// ?? add code here
	printf("\nfmsSeekFile Not Implemented");

	return ERR63;
} // end fmsSeekFile



// ***********************************************************************
// ***********************************************************************
// This function writes nBytes bytes to the open file specified by fileDescriptor from
// memory pointed to by buffer.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
// Writing is always "overwriting" not "inserting" in the file and always writes forward
// from the current file pointer position.
// Return the number of bytes successfully written; otherwise, return the error number.
//
int fmsWriteFile(int fileDescriptor, char* buffer, int nBytes)
{
	int error;
    int nextCluster, endCluster;
    FDEntry* fdEntry;
    int numBytesWritten = 0;
    unsigned int bytestLeft, bufferIndex;
    fdEntry = &OFTable[fileDescriptor];
    if (fdEntry->name[0] == 0) return ERR63; // file not open
    if (fdEntry->mode == OPEN_READ) return ERR85; //illegal access

    while (nBytes > 0) {
        bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;
        if ((bufferIndex == 0) && (fdEntry->fileIndex || !fdEntry->currentCluster)) {
            if (fdEntry->currentCluster == 0) { //file has been lazy opened and buffer is not initialized
                if (fdEntry->startCluster == 0) {
                    if ((nextCluster = getFreeCluster()) < 2) return nextCluster;
                    fdEntry->startCluster = (uint16) nextCluster;
                    //printf("\nAssigning cluster %d to new file %d", cluster, fileDescriptor);
                    setFatEntry(nextCluster, FAT_EOC, FAT1);
                }
                nextCluster = fdEntry->startCluster;
                fdEntry->fileIndex = 0;
            }
            else {
                nextCluster = getFatEntry(fdEntry->currentCluster,FAT1);
                if (nextCluster == FAT_EOC) {
                    //we have reached the end of the cluster but still have more to write
                    //get a new cluster and append it to the end of the cluster chain
                    if ((nextCluster = getFreeCluster())) {
                        if (nextCluster < 2) {
                            return nextCluster; // error
                        }
                        setFatEntry(fdEntry->currentCluster,nextCluster, FAT1);
                        setFatEntry(nextCluster,FAT_EOC, FAT1);
                    }

                }
            }
            if (fdEntry->flags & BUFFER_ALTERED) {
                if ((error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) {
                    return error;
                }
                fdEntry->flags &= ~BUFFER_ALTERED;
            }
            fdEntry->currentCluster = nextCluster;
        }
        if((error = fmsReadSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster)))) return error;

        bytestLeft = BYTES_PER_SECTOR - bufferIndex;
        if (bytestLeft > nBytes) {
            bytestLeft = nBytes;
        }
        memcpy(&fdEntry->buffer[bufferIndex], buffer, bytestLeft);
        fdEntry->fileIndex += bytestLeft;
        numBytesWritten += bytestLeft;
        buffer += bytestLeft;
        nBytes -= bytestLeft;
        fdEntry->flags |= BUFFER_ALTERED;
        fdEntry->fileSize = fdEntry->fileIndex;
    }

    endCluster = fdEntry->currentCluster;
    while ((nextCluster = getFatEntry(endCluster,FAT1)) != FAT_EOC) {
//        printf("\nNext Cluster %d", nextCluster);
//        we have completed writing and the chain hasn't ended
//        set the further links free
        setFatEntry(endCluster,0x00,FAT1);
        endCluster = nextCluster;
    }
    setFatEntry(fdEntry->currentCluster,FAT_EOC,FAT1);

    return numBytesWritten;
} // end fmsWriteFile


/**
 * A 12-bit FAT entry may have the following values
 * 0x000 Unused cluster.
 * 0xFF0-0xFF6 Reserved cluster (like FAT entries 0 and 1).
 * 0xFF7 Bad Cluster (contains errors and should not be used).
 * 0xFF8-0xFFF End of file/directory, also called EOC (end of cluster chain).
 * other numbers are pointers (indexes) to the next cluster in the file/directory.
 */
int getFreeCluster()
{
    int cluster, error, sector;
    char buffer[BYTES_PER_SECTOR];
    memset(buffer, 0, BYTES_PER_SECTOR * sizeof(char));

    for (cluster = 2; cluster < CLUSTERS_PER_DISK; cluster++)
    {

        if (!getFatEntry(cluster, FAT1)) // if its an Unused cluster
        {
            // overwrite any data that may have been in the sector associated with cluster
            sector = C_2_S(cluster);
            if ((error = fmsWriteSector(buffer, sector)) < 0) return error;
            return cluster;
        }
    }

    return ERR65; // No empty entries found, so file space is full
}

// ***************************************************************************************
// ***************************************************************************************
int fmsUpdateDirEntry(FDEntry* fdEntry)
//  This function finds a dirEntry relating to the fdEntry and updates it accordingly
{

    int dirNum = 0,dirIndex, error, dirSector;
    char fileName[32];
    char buffer[BYTES_PER_SECTOR];
    strcpy(fileName, fdEntry->name);
    DirEntry dirEntry;
    if((error = fmsGetNextDirEntry(&dirNum,fileName,&dirEntry,CDIR))){
        return error;
    }

    dirNum--; //fmsGetNextDirEntry increments preemptively
    dirIndex = dirNum % ENTRIES_PER_SECTOR; //same as process in fmsGetNextDir
    // dirNum / ENTRIES_PER_SECTOR gives you which sector but you have to add offset to get to sector area in FAT
    dirSector = dirNum / ENTRIES_PER_SECTOR + BEG_ROOT_SECTOR;

    //Update dirEntry potential changes
    dirEntry.fileSize = fdEntry->fileSize;
    dirEntry.startCluster = fdEntry->startCluster;
    dirEntry.attributes = fdEntry->attributes;
    setDirTimeDate(&dirEntry);
    //Read in entire sector
    if ((error = fmsReadSector(buffer, dirSector))) {
        printf("\nErrored in reading sector");
        return error;
    }
    //update dirEntiry in sector
    memcpy(&buffer[dirIndex * sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
    //write back entire sector
    if ((error = fmsWriteSector(&buffer, dirSector)) < 0) {
        printf("\nErrored in writing sector");
        return error;
    };

    //release fdEntry
    fdEntry->name[0] = 0;

    return 0;
} // end fmsGetNextDirEntry