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
#include <time.h>
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


// helper - checkValidClose
int checkValidClose(FDEntry* fdentry)
{
	int error;
	if (fdentry->name[0] == 0)
		return ERR63;

	if (fdentry->pid != curTask)
		return ERR85;

	return 0;
} // end checkValidClose


// helper setDirEntry
int setDirEntry(DirEntry* dirEntry)
{
	int dirSector, error, i, j, k;
	char buffer[BYTES_PER_SECTOR];

	// read currentDirectorySector into buffer
	dirSector = C_2_S(CDIR);
	if (error = fmsReadSector(buffer, dirSector)) return error;

	// replace dirEntry in buffer
	for (i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		j = i * sizeof(DirEntry);
		for (k = 0; k < 8; k++, j++)
		{
			if (buffer[j] != dirEntry->name[k])
				break;
		}
		if (k == 8)
			break;
		
	}
	memcpy(&buffer[i * sizeof(DirEntry)], dirEntry, sizeof(DirEntry));

	// write buffer out to sector
	if (error = fmsWriteSector(buffer,C_2_S(CDIR))) return error;

	return 0;
} // end setDirEntry

// helper - updateDirEntry
int updateDirEntry(FDEntry* fdentry)
{
	int error, i, j;
	time_t a;
	struct tm *b;
	FATDate *d;
	FATTime *t;
	DirEntry dirEntry;
	char name[12];


	for (i = 0; i < 8; i++)
	{
		if (fdentry->name[i] != ' ')
		{
			name[i] = fdentry->name[i];
		}
		else
		{
			break;
		}
	}

	if (fdentry->extension[0] != ' ')
		name[i++] = '.';
	else
		name[i++] = ' ';

	for (j = 0; j < 3; i++,j++)
	{
		name[i] = fdentry->extension[j];
	}
	name[i] = 0;
	

	//printf(name);

	if (error = fmsGetDirEntry(name, &dirEntry)) return error;

	// capture local time and date
	time(&a);
	b = localtime(&a);	// get local time 

	d = &dirEntry.date;	// point to date w/in dir entry
	d->year = b->tm_year + 1900 - 1980; // update year
	d->month = b->tm_mon;	// update month
	d->day = b->tm_mday;	// update day 
	//dirEntry.date.year

	t = &dirEntry.time;	// point to time w/in dir entry
	t->hour = b->tm_hour;	// update hour
	t->min = b->tm_min;	// update minute
	t->sec = b->tm_sec;	// update second

	dirEntry.fileSize = fdentry->fileSize;
	dirEntry.startCluster = fdentry->startCluster;		// it would have changed in a new file

	setDirEntry(&dirEntry);

	return 0;
} // end updateDirEntry

// ***********************************************************************
// ***********************************************************************
// This function closes the open file specified by fileDescriptor.
// The fileDescriptor was returned by fmsOpenFile and is an index into the open file table.
//	Return 0 for success, otherwise, return the error number.
//
int fmsCloseFile(int fileDescriptor)
{
	// ?? add code here
	//printf("\nfmsCloseFile Not Implemented");

	//return ERR63;

	int error;
	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// check for file not open and illegal access errors
	if (error = checkValidClose(fdEntry)) return error;

	// flush transaction buffer if altered
	if (fdEntry->flags & BUFFER_ALTERED)
	{
		error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster));
		if (error) return error;
	}

	// update directory if file altered
	if ((fdEntry->flags & FILE_ALTERED) && (error = updateDirEntry(fdEntry))) return error;

	// release open file slot
	fdEntry->name[0] = '\0';

	return 0;
} // end fmsCloseFile

// helper createDirEntry
int createDirEntry(DirEntry* dirEntry, char* fileName, int attribute)
{
	time_t a;
	struct tm *b;
	FATDate *d;
	FATTime *t;
	int error, i, len, j, k;

	len = strlen(fileName);
	if (8 < len)
		len = 8;
	/*
	for (i = 0; i < len; i++)
	{
		if (fileName[i] == '.')
		{
			for (j = i; j < 8; j++)
			{
				dirEntry->name[j] = ' ';
			}
			break;
		}
		else
			dirEntry->name[i] = toupper(fileName[i]);
	}
	*/
	if (fileName[0] == '.')
	{
		dirEntry->name[0] = '.';
		if (fileName[1] == '.')
		{
			dirEntry->name[1] = '.';
			i = 2;
		}
		else
			i = 1;
	}
	else
	{
		for (i = 0; i < len; i++)
		{
			if (fileName[i] == '.')
			{
				break;
			}
			else
				dirEntry->name[i] = toupper(fileName[i]);
		}
	}
	for (j = i; j < 8; j++)
		dirEntry->name[j] = ' ';

	//i++;
	if (fileName[i] == '.')
	{
		i++;
		for (k=0; k < 3; k++,i++)
		{
			dirEntry->extension[k] = toupper(fileName[i]);
		}
	}
	else
	{
		for (k=0; k < 3; k++)
		{
			dirEntry->extension[k] = ' ';
		}
	}
	
	// set the attribute
	dirEntry->attributes = attribute;

	// set the reseved[10] to 0
	for (i = 0; i < 10; i++)
		dirEntry->reserved[i] = 0;

	// capture local time and date
	time(&a);
	b = localtime(&a);	// get local time 

	// set the time
	t = &dirEntry->time;	// point to time w/in dir entry
	t->hour = b->tm_hour;	// update hour
	t->min = b->tm_min;	// update minute
	t->sec = b->tm_sec;	// update second

	// set the date
	d = &dirEntry->date;	// point to date w/in dir entry
	d->year = b->tm_year + 1900 - 1980; // update year
	d->month = b->tm_mon;	// update month
	d->day = b->tm_mday;	// update day 

	// set startCluster to 0
	dirEntry->startCluster = 0;

	// set fileSize to 0
	dirEntry->fileSize = 0;

	return 0;
} // end createDirEntry

// helper getFreeDirEntry
int getFreeDirEntry(int* dirIndex, int* dirSector, int cluster)
{
	int error, i, j;
	char buffer[BYTES_PER_SECTOR];

	// read currentDirectorySector into buffer
	if (CDIR == 0)
		*dirSector = 19 + cluster;
	else
		*dirSector = C_2_S(cluster);
	if (error = fmsReadSector(buffer, *dirSector)) return error;

	// find empty or deleted dirEntry in buffer
	for (i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		j = i * sizeof(DirEntry);
		if ((buffer[j] == 0) || (buffer[j] == 0xe5))
		{
			*dirIndex = i;
			return 0;
		}
	}

	// got to the end? directory full, need to grow
	if (CDIR != 0)
	{
		int nextCluster;

		nextCluster = getFatEntry(CDIR, FAT1);
		if (nextCluster == FAT_EOC)
		{
			char buffer[512];
			int newCluster = getFreeFATEntry();
			setFatEntry(newCluster, FAT_EOC, FAT1);
			setFatEntry(cluster, newCluster, FAT1);
			for (i = 0; i < 512; i++)
				buffer[0] = 0;

			if (error = fmsWriteSector(buffer, *dirSector)) return error;
			
			getFreeDirEntry(dirIndex,dirSector,newCluster);

			return 0;
		}
		else
		{
			getFreeDirEntry(dirIndex,dirSector,nextCluster);

			return 0;
		}

		//return ERR64;
	}
	else	// root can't grow, but it has more clusters
	{
		for (*dirSector = 20; *dirSector < 33; *dirSector++)
		{
			if (error = fmsReadSector(buffer, *dirSector)) return error;
			// find empty or deleted dirEntry in buffer
			for (i = 0; i < ENTRIES_PER_SECTOR; i++)
			{
				j = i * sizeof(DirEntry);
				if ((buffer[j] == 0) || (buffer[j] == 0xe5))
				{
					*dirIndex = i;
					return 0;
				}
			}
		}
		return ERR64;		
	}
} // end getFreeDirEntry

// helper createDirectory
int createDirectory(char* buffer, int cluster)
{
	DirEntry dot;
	DirEntry dotdot;
	char cdot[2];
	char cdotdot[3];
	int i;

	cdot[0] = cdotdot[0] = cdotdot[1] = '.';
	cdot[1] = cdotdot[2] = 0;
	
	for (i = 0; i < 512; i++)
		buffer[i] = 0;

	createDirEntry(&dot, cdot, DIRECTORY);
	dot.startCluster = cluster;		// self

	createDirEntry(&dotdot, cdotdot, DIRECTORY);
	dotdot.startCluster = CDIR;		// parent

	memcpy(buffer, &dot, sizeof(DirEntry));
	memcpy(&buffer[1 * sizeof(DirEntry)], &dotdot, sizeof(DirEntry));

	return 0;
} // end createDirectory

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
	//printf("\nfmsDefineFile Not Implemented");

	//return ERR72;

	char buffer[BYTES_PER_SECTOR];
	int dirIndex, dirCluster, dirSector, dirNum;
	int error, nextCluster, i, j;
	DirEntry dirEntry;
	int FID, freeCluster;

	// error if file already defined
	if (!(error = fmsGetDirEntry(fileName, &dirEntry))) return ERR60;
	else if (error != ERR61) return error;

	if (attribute == DIRECTORY)
	{
		// check fileName
		if (isValidFileName(fileName) < 1) return ERR50;

		// create new directory entry
		if (error = createDirEntry(&dirEntry, fileName, attribute)) return error;

		// get fat entry and cluster to write directory to
		freeCluster = getFreeFATEntry();

		setFatEntry(freeCluster, FAT_EOC, FAT1);

		dirEntry.startCluster = freeCluster;

		// create new directory cluster and write it out to disk
		if (error = createDirectory(buffer, freeCluster)) return error;
		if (error = fmsWriteSector(buffer, C_2_S(freeCluster))) return error;

		// write the sub-directory to the directory's cluster
		// get first free directory opening
		if (error = getFreeDirEntry(&dirIndex, &dirSector, CDIR)) return error;
		// OK, update directory entry
		if (error = fmsReadSector(buffer, dirSector)) return error;
		memcpy(&buffer[dirIndex*sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));
		// write sector back
		if (error = fmsWriteSector(buffer, dirSector)) return error;
	}
	else
	{
		// check fileName
		if (isValidFileName(fileName) < 1) return ERR50;

		// create new directory entry
		if (error = createDirEntry(&dirEntry, fileName, attribute)) return error;

		// get first free directory opening
		if (error = getFreeDirEntry(&dirIndex, &dirSector, CDIR)) return error;

		// OK, update directory entry
		if (error = fmsReadSector(buffer, dirSector)) return error;

		memcpy(&buffer[dirIndex*sizeof(DirEntry)], &dirEntry, sizeof(DirEntry));

		// write sector back
		if (error = fmsWriteSector(buffer, dirSector)) return error;

	}
	return 0;
} // end fmsDefineFile

// helper findDirEntry
int delDirEntry(DirEntry* dirEntry)
{
	int dirSector, error, i, j, k;
	char buffer[BYTES_PER_SECTOR];

	// read currentDirectorySector into buffer
	dirSector = C_2_S(CDIR);
	if (error = fmsReadSector(buffer, dirSector)) return error;

	// replace dirEntry in buffer
	for (i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		j = i * sizeof(DirEntry);
		for (k = 0; k < 8; k++, j++)
		{
			if (buffer[j] != dirEntry->name[k])
				break;
		}
		if (k == 8)
			break;
		
	}
	dirEntry->name[0] = 0xe5;
	memcpy(&buffer[i * sizeof(DirEntry)], dirEntry, sizeof(DirEntry));

	// write buffer out to sector
	if (error = fmsWriteSector(buffer,C_2_S(CDIR))) return error;

	return 0;
} // end findDirEntry

// helper getDirectoryEntries
int getDirectoryEntries(int startCluster)
{
	int error, i, j, len, dirSector;
	char buffer[BYTES_PER_SECTOR];

	// read currentDirectorySector into buffer
	dirSector = C_2_S(startCluster);
	if (error = fmsReadSector(buffer, dirSector)) return error;

	// if find a defined file/directory, this directory isn't empty
	for (i = 0; i < ENTRIES_PER_SECTOR; i++)
	{
		j = i * sizeof(DirEntry);
		
		if (!((buffer[j] == 0) || buffer[j] == 0xe5))
			return ERR69;
	}
	// got to the end? no defined files
	return 0;
} // getDirectoryEntries

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
	//printf("\nfmsDeleteFile Not Implemented");

	//return ERR61;
	int error, dirIndex = 0;
	int dirSector, cluster, nextCluster;
	char buffer[BYTES_PER_SECTOR];
	DirEntry dirEntry;


	if (error = fmsGetDirEntry(fileName, &dirEntry)) return error;

	if ((dirEntry.attributes == DIRECTORY) &&
		(error = getDirectoryEntries(dirEntry.startCluster))) return error;

	if (error = delDirEntry(&dirEntry)) return error;

	// free up allocated clusters
	if (cluster = dirEntry.startCluster)
	{
		do
		{
			nextCluster = getFatEntry(cluster, FAT1);
			setFatEntry(cluster, 0, FAT1);
			cluster = nextCluster;
		} while ((cluster != FAT_EOC) && (cluster != FAT_BAD));
	}

	return 0;

} // end fmsDeleteFile

// helper - checkValidReadFile
int checkValidReadFile(char* fileName, int rwMode, DirEntry* dirEntry)
{
	int error;

	if (error = isValidFileName(fileName) < 1) return ERR50;

	if (error = fmsGetDirEntry(fileName, dirEntry)) return error;

	if ((rwMode < 0) || (rwMode > 3)) return -1;

	if (dirEntry->attributes == 0x10) return -1;

	return 0;
} // end checkValidReadFile

// helper - findFileSlot
int findFileSlot(int* fileDescriptor, DirEntry* dirEntry)
{
	int error,i;

	for (i = 0; i < NFILES; i++)
	{
		// same start cluster = same file
		if ((OFTable[i].name[0] != 0) && (OFTable[i].startCluster == dirEntry->startCluster))
			return ERR62;
		else if (OFTable[i].name[0] == 0)
		{
			*fileDescriptor = i;
			return 0;
		}
	}

	return ERR70;

	return 0;
} // end findFileSlot

// helper - populateFileDescriptor
int populateFileDescriptor(int* fileDescriptor, DirEntry* dirEntry, int rwMode)
{
	int i,error;
	FDEntry* temp = &OFTable[*fileDescriptor];
	uint16 nextCluster;

	for (i=0; i<12; i++) temp->name[i] = dirEntry->name[i];		// name and extension in one go
	temp->attributes = dirEntry->attributes;
	temp->directoryCluster = CDIR;
	temp->startCluster = dirEntry->startCluster;
	temp->currentCluster = 0;
	temp->fileSize = (rwMode == 1) ? 0 : dirEntry->fileSize;	// for write zero, else current size
	temp->pid = curTask;
	temp->mode = rwMode;
	temp->flags = 0;
	temp->fileIndex = (rwMode != 2) ? 0 : dirEntry->fileSize;		// for append end of file, else beginning
	for (i=0; i<BYTES_PER_SECTOR; i++) temp->buffer[i] = 0;


	// if rwMode 2, go to end cluster, else stay at beginning, load buffer
	temp->currentCluster = temp->startCluster;
	if (rwMode == 2)
	{
		while ((nextCluster = getFatEntry(temp->currentCluster,FAT1)) != FAT_EOC)
		     temp->currentCluster = nextCluster;
	}
	if ((error = fmsReadSector(temp->buffer,
		temp->currentCluster + 31))) return error;	// sector is cluster + 31



	return 0;
} // end populateFileDescriptor

// helper - positionToEOF
int positionToEOF()
{
	return 0;
} // end positionToEOF

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
	// ?? add code here
	//printf("\nfmsOpenFile Not Implemented");
	//return ERR61;

	int error, fileDescriptor;
	DirEntry dirEntry;

	// check for invalid file name, undefined, invalid mode, or directory
	if (error = checkValidReadFile(fileName, rwMode, &dirEntry)) return error;

	// find available file descriptor (check already open, too many open)
	if (error = findFileSlot(&fileDescriptor, &dirEntry)) return error;

	// OK, open file (populate file descriptor)
	if (error = populateFileDescriptor(&fileDescriptor, &dirEntry, rwMode)) return error;

	// the next function is not actually necessary as i change the file index in populateFileDescriptor if rwMode is 2
	// if append access, move file pointer to end of file
	//if ((rwMode == 2) && (error = positionToEOF(&OFTable[fileDescriptor]))) return error;

	return fileDescriptor;
} // end fmsOpenFile

// helper - checkValidRead
int checkValidRead(FDEntry* fdentry)
{
	int error;
	if (fdentry->name[0] == 0)
		return ERR63;

	if (!((fdentry->mode == 0) || (fdentry->mode == 3)) || (fdentry->pid != curTask))
		return ERR85;

	return 0;
} // end checkValidRead

// helper getFreeFATEntry
int getFreeFATEntry()
{
	int i;
	int numFATEntries = (BYTES_PER_SECTOR * NUM_FAT_SECTORS) / 1.5;
	for (i = 2; i < numFATEntries; i++)
	{
		if (getFatEntry(i, FAT1) == 0)
			return i;
	}

	return ERR65;
} // end getFreeFATEntry

// helper - getValidBufferIndex
int getValidBufferIndex(int fileDescriptor, int bufferIndex, int write)
{
	int i, numClustersToMove, error, nextCluster, newFATEntry;
	FDEntry* temp = &OFTable[fileDescriptor];

	temp->currentCluster = temp->startCluster;
	numClustersToMove = temp->fileIndex / BYTES_PER_SECTOR;

	//printf("clusters to move: %d", numClustersToMove);

	for (i = 0; i < numClustersToMove; i++)
	{
		nextCluster = getFatEntry(temp->currentCluster,FAT1);
		if (nextCluster == FAT_EOC)
		{
			
			// if reading, asking for information from EOC is error
			if (!write)
			{
				return ERR66;
			}
			else
			{
				// if writing, FAT_EOC indicates a need to extend
				newFATEntry = getFreeFATEntry();

				setFatEntry(newFATEntry, FAT_EOC, FAT1);
				setFatEntry(temp->currentCluster, newFATEntry, FAT1);

				temp->currentCluster = newFATEntry;
			}
		}
		else
			temp->currentCluster = nextCluster;
	}

	//while ((nextCluster = getFatEntry(temp->currentCluster,FAT1)) != FAT_EOC)
    //     temp->currentCluster = nextCluster;

	if ((error = fmsReadSector(temp->buffer,
		temp->currentCluster + 31))) return error;	// sector is cluster + 31

	return 0;
} // end getValidBufferIndex

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
	// ?? add code here
	//printf("\nfmsReadFile Not Implemented");

	//return ERR63;

	int error;
	unsigned int bytesLeft, bufferIndex;
	int numBytesRead = 0;
	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// check for file not open, illegal access
	if (error = checkValidRead(fdEntry)) return error;

	// read nBytes from file into buffer
	while (nBytes > 0)
	{
		// check for EOF
		if (fdEntry->fileSize == fdEntry->fileIndex)
			return (numBytesRead ? numBytesRead : ERR66);

		// get valid file descriptor buffer (correct index)
		bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;
		if (error = getValidBufferIndex(fileDescriptor, bufferIndex, 0)) return error;

		// limit bytes to read to min(bytes in buffer, remaining file bytes, nBytes)
		bytesLeft = BYTES_PER_SECTOR - bufferIndex;
		if (bytesLeft > nBytes) bytesLeft = nBytes;
		if (bytesLeft > (fdEntry->fileSize - fdEntry->fileIndex))
			bytesLeft = fdEntry->fileSize - fdEntry->fileIndex;

		// move data from internal buffer to user buffer and update counts
		memcpy(buffer, &fdEntry->buffer[bufferIndex], bytesLeft);
		fdEntry->fileIndex += bytesLeft;
		numBytesRead += bytesLeft;
		buffer += bytesLeft;
		nBytes -= bytesLeft;
	}
	return numBytesRead;
	
} // end fmsReadFile

// helper checkValidSeek
int checkValidSeek(FDEntry* fdentry)
{
	int error;
	if (fdentry->name[0] == 0)
		return ERR63;

	if (fdentry->pid != curTask)				// wrong pid
		return ERR85;

	return 0;
} // end checkValidSeek

// helper getSeekClusterNumber
int getSeekClusterNumber(int* cluster, int index, int fileDescriptor)
{
	int i, numClustersToMove, error, nextCluster, newFATEntry;
	FDEntry* temp = &OFTable[fileDescriptor];

	*cluster = temp->startCluster;
	numClustersToMove = index / BYTES_PER_SECTOR;

	//printf("clusters to move: %d", numClustersToMove);

	for (i = 0; i < numClustersToMove; i++)
	{
		nextCluster = getFatEntry(temp->currentCluster,FAT1);
		if (nextCluster != FAT_EOC)
			*cluster = nextCluster;
		else
			return ERR66;
	}

	return 0;
} // end getSeekClusterNumber

// helper readSeekCluster
int readSeekCluster(FDEntry* fdEntry, int cluster)
{
	int error;
	if ((error = fmsReadSector(fdEntry->buffer,
		cluster + 31))) return error;	// sector is cluster + 31

	fdEntry->currentCluster = cluster;

	return 0;
} // end readSeekCluster

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
	//printf("\nfmsSeekFile Not Implemented");

	//return ERR63;

	int error, cluster;
	unsigned int clusterIndex, clusterBufferIndex;
	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// check for file not open and illegal access errors
	if (error = checkValidSeek(fdEntry)) return error;
	
	// check to see if positioning after eof
	if (index > fdEntry->fileSize) return ERR66;   // end of file

	// find desired cluster/index
	if (error = getSeekClusterNumber(&cluster, index, fileDescriptor)) return error;

	// get cluster into buffer (if different than currentCluster)
	if (error = readSeekCluster(fdEntry, cluster)) return error;

	// correct cluster in memory, update file index
	fdEntry->fileIndex = index;

	return index;

} // end fmsSeekFile

// helper - checkValidWrite
int checkValidWrite(FDEntry* fdentry)
{
	int error;
	if (fdentry->name[0] == 0)
		return ERR63;

	if (!((fdentry->mode == 1) || (fdentry->mode == 2) || (fdentry->mode == 3))		// if not a writable mode
		|| (fdentry->attributes == 0x01) || (fdentry->pid != curTask))				// read-only, or wrong pid
		return ERR85;

	return 0;
} // end checkValidWrite

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
	// ?? add code here
	//printf("\nfmsWriteFile Not Implemented");

	//return ERR63;

	int error, i, fd;
	unsigned int bytesLeft, bufferIndex;

	int numBytesWritten = 0;
	FDEntry* fdEntry = &OFTable[fileDescriptor];

	// check for file not open, illegal access
	if (error = checkValidWrite(fdEntry)) return error;

	// get a new FAT cluster to write to if a new file
	if (fdEntry->startCluster == 0)
	{
		int newStart = getFreeFATEntry();

		setFatEntry(newStart, FAT_EOC, FAT1);

		fdEntry->startCluster = newStart;
		//setFatEntry(fdEntry->startCluster, newStart, FAT1);
	}

	// ok write to file
	while (nBytes > 0)
	{
		// get valid file descriptor buffer (correct index)
		bufferIndex = fdEntry->fileIndex % BYTES_PER_SECTOR;
		if (error = getValidBufferIndex(fileDescriptor, bufferIndex, 1)) return error;

		// get number of bytes to write in buffer
		bytesLeft = BYTES_PER_SECTOR - bufferIndex;
		if (bytesLeft > nBytes) bytesLeft = nBytes;

		// write to buffer
		memcpy(&fdEntry->buffer[bufferIndex], buffer, bytesLeft);

		// update pointers and counters
		buffer += bytesLeft;                            // prepare buffer for next write
		nBytes -= bytesLeft;                            // decrement total write count
		numBytesWritten += bytesLeft;                   // increment total bytes written

		// update file index and size (if extending file)
		fdEntry->fileIndex += bytesLeft;
		if (fdEntry->fileIndex > fdEntry->fileSize) fdEntry->fileSize = fdEntry->fileIndex;

		// file and buffer have been altered
		fdEntry->flags |= (FILE_ALTERED | BUFFER_ALTERED);

		// write-out buffer to file
		error = fmsWriteSector(fdEntry->buffer, C_2_S(fdEntry->currentCluster));
		if (error) return error;

	}
	return numBytesWritten;
} // end fmsWriteFile
