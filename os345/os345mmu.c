// os345mmu.c - LC-3 Memory Management Unit
// **************************************************************************
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include "os345.h"
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int nextPage;						// swap page size
int pageReads;						// page reads
int pageWrites;						// page writes

int getFrame(int);
int getAvailableFrame(void);

int curRPTE;
int curUPTE;
int clockCount;

int clockRunner(int notme)
{
	//extern int curRPT;
	//extern TCB tcb[MAX_TASKS];
	int i,j,k;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int victimFrame;
	int tupta, tupte1;

	//printf("\nClock running...%d\n",clockCount++);
	//victimEntry = 0;

	while (1)
	{
		//rpta = tcb[curRPT].RPT;
		for (i = curRPTE; i < 0x3000; i+=2)
		{
			//rpte1 = memory[rpta + i];
			//rpte2 = memory[rpta + i + 1];
			rpte1 = memory[i];
			rpte2 = memory[i + 1];
			
			if (DEFINED(rpte1))
			{
				if (curUPTE == 0)
				{
					if(PINNED(rpte1))
					{
						//memory[rpta + i] = rpte1 = CLEAR_PINNED(rpte1);
						memory[i] = rpte1 = CLEAR_PINNED(rpte1);
					}

				}

				upta = (FRAME(rpte1)<<6);

				for (j = curUPTE; j < 64; j+=2)
				{
					upte1 = memory[upta + j];
					upte2 = memory[upta + j + 1];

					if (DEFINED(upte1))
					{
						//memory[rpta + i] = rpte1 = SET_PINNED(rpte1);
						memory[i] = rpte1 = SET_PINNED(rpte1);

						if (REFERENCED(upte1))
						{
							memory[upta + j] = upte1 = CLEAR_REF(upte1);
						}
						else
						{
							//victimEntry = upte1;
							// if end of UPT, reset upte and move to next UPT
							//break;	// because this is the victim
							// instead of break, do swapping and return victim frame number

							// immunity from notme
							//if (FRAME(upte1) != notme)
							//{ 	
							
								if (DIRTY(upte1))
								{
									int page;

									//stat
									pageWrites++;

									if(PAGED(upte2))
									{
										page = accessPage(SWAPPAGE(upte2),FRAME(upte1),PAGE_OLD_WRITE);
									}
									else
									{
										page = accessPage(0,FRAME(upte1),PAGE_NEW_WRITE);
									}

									upte2 = page;
									memory[upta + j + 1] = upte2 = SET_PAGED(upte2);
									
								}


								// save frame number before obliterating first word
								victimFrame = FRAME(upte1);
								//memory[upta + j] = upte1 = CLEAR_DEFINED(upte1);
								memory[upta + j] = upte1 = 0;

								curUPTE = (j + 2) % 64;

								//printf("\nChose a UPT victim: %d\n",victimFrame);
								return victimFrame;
							//}
						}
					}
				}

				// immunity from notme
				if ((FRAME(rpte1) != notme) && (!PINNED(rpte1)))
				{
					//victimEntry = rpte1;
					// if end of RPT, reset rpte and move to next RPT
					//break;		// because this is the victim
					// instead of break, do swapping and return victim frame number

					//victimEntry = FRAME(rpte1);
						

					// sanity check
					tupta = (FRAME(rpte1)<<6);
					for (k = 0; k < 64; k+=2)
					{
						tupte1 = memory[tupta + k];
						if (DEFINED(tupte1))
							printf("\nEvicting a UPT that has defined entries!!!\n");
					}


					//if (DIRTY(rpte1))
					{
						int page;

						//stat
						pageWrites++;

						if(PAGED(rpte2))
						{
							page = accessPage(SWAPPAGE(rpte2),FRAME(rpte1),PAGE_OLD_WRITE);
						}
						else
						{
							page = accessPage(0,FRAME(rpte1),PAGE_NEW_WRITE);
						}

						rpte2 = page;
						//memory[rpta + i + 1] = rpte2 = SET_PAGED(rpte2);
						memory[i + 1] = rpte2 = SET_PAGED(rpte2);
						
					}

					// save frame number before obliterating first word
					victimFrame = FRAME(rpte1);
					//memory[rpta + i] = rpte1 = CLEAR_DEFINED(rpte1);
					//memory[rpta + i] = rpte1 = 0;
					memory[i] = rpte1 = 0;

					//curRPTE = (i + 2) % 64;

					//printf("\nChose a RPT victim: %d\n",victimFrame);
					return victimFrame;
				}

				curUPTE = 0;
			}

		}

		//curRPT = (curRPT + 1) % MAX_TASKS;
		curRPTE = 0x2400;
	}


}

int getFrame(int notme)
{
	int frame;
	frame = getAvailableFrame();
	if (frame >=0) return frame;

	// run clock
	//printf("\nWe're toast!!!!!!!!!!!!");
	frame = clockRunner(notme);

	return frame;
}
// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	             / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;
	extern TCB tcb[MAX_TASKS];
	extern int curTask;

	//stat
	memAccess++;

	rpta = tcb[curTask].RPT + RPTI(va);	// 0x2400 + RPTI(va);
	rpte1 = memory[rpta];
	rpte2 = memory[rpta+1];

	// turn off virtual addressing for system RAM
	if (va < 0x3000) return &memory[va];
#if MMU_ENABLE
	if (DEFINED(rpte1))
	{
		// defined
		//stat
		memHits++;
		if (rwFlg)
			rpte1 = SET_DIRTY(rpte1);
	}
	else
	{
		// fault
		//stat
		memPageFaults++;

		rptFrame = getFrame(-1);
		rpte1 = SET_DEFINED(rptFrame);
		if (PAGED(rpte2))
		{
			accessPage(SWAPPAGE(rpte2), rptFrame, PAGE_READ);
			//stat
			pageReads++;
			//clean
			if (rwFlg)
				rpte1 = SET_DIRTY(rpte1);
			else
				rpte1 = CLEAR_DIRTY(rpte1);
		}
		else
		{
			memset(&memory[(rptFrame<<6)], 0, 128);
			//dirty
			rpte1 = SET_DIRTY(rpte1);
		}
	}

	
	rpte1 = SET_PINNED(rpte1);
	memory[rpta] = rpte1 = SET_REF(rpte1);
	memory[rpta+1] = rpte2;

	upta = (FRAME(rpte1)<<6) + UPTI(va);
	upte1 = memory[upta];
	upte2 = memory[upta+1];

	//stat
	memAccess++;

	if (DEFINED(upte1))
	{
		// defined
		//stat
		memHits++;
		if (rwFlg)
			upte1 = SET_DIRTY(upte1);
	}
	else
	{
		// fault
		//stat
		memPageFaults++;

		uptFrame = getFrame(FRAME(memory[rpta]));
		upte1 = SET_DEFINED(uptFrame);
		if (PAGED(upte2))
		{
			accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
			//stat
			pageReads++;
			//clean unless being written to
			if (rwFlg)
				upte1 = SET_DIRTY(upte1);
			else
				upte1 = CLEAR_DIRTY(upte1);
		}
		else
		{
			//dirty
			upte1 = SET_DIRTY(upte1);
		}
	}

	rpte1 = SET_PINNED(rpte1);
	memory[rpta] = rpte1;
	memory[upta] = upte1 = SET_REF(upte1);
	memory[upta+1] = upte2;


	return &memory[(FRAME(upte1)<<6) + FRAMEOFFSET(va)];
#else
	return &memory[va];
#endif
} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{	int i, data;
	int adr = LC3_FBT-1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i=0; i<LC3_FRAMES; i++)
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;
			adr++;
			data = (flg)?MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ( (i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i=0; i<LC3_FRAMES; i++)		// look thru all frames
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{  MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
   static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

   if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
   {
      printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
      exit(-4);
   }
   switch(rwnFlg)
   {
      case PAGE_INIT:                    		// init paging
         nextPage = 0;
         return 0;

      case PAGE_GET_ADR:                    	// return page address
         return (int)(&swapMemory[pnum<<6]);

      case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
         pnum = nextPage++;

      case PAGE_OLD_WRITE:                   // write
         //printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
         memcpy(&swapMemory[pnum<<6], &memory[frame<<6], 1<<7);
         pageWrites++;
         return pnum;

      case PAGE_READ:                    // read
         //printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
      	memcpy(&memory[frame<<6], &swapMemory[pnum<<6], 1<<7);
         pageReads++;
         return pnum;

      case PAGE_FREE:                   // free page
         break;
   }
   return pnum;
} // end accessPage
