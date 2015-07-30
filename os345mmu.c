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
int cBigHand;
int cLittleHand;

int getFrame(int);
int getClockFrame(int);
int getAvailableFrame(void);
void swapOutFrame(int entry1Index);
extern TCB tcb[];					// task control block
extern int curTask;					// current task #


int getFrame(int notme)
{
	int frame;
	frame = getAvailableFrame();
	if (frame >=0) return frame;

	// run clock
    frame = getClockFrame(notme);

	return frame;
}

int getClockFrame(int notme)
{
    int frame;

    // iterate through rpts (0x2400 - LC3_RPT_END)
    int maxWrap = 20;
    for (;maxWrap; cBigHand += 2, cLittleHand = 0) {
        int i;
        int rpte1;
        int upta, upte1;

        if (cBigHand >= LC3_RPT_END) {
            DEBUG_STATEMENT(DEBUG_MMU,"\nClock is wrapping");
            cBigHand = LC3_RPT;
            maxWrap--;
        }

        rpte1 = memory[cBigHand];

        if (DEFINED(rpte1) && REFERENCED(rpte1)) {
            // clear reference
            DEBUG_STATEMENT(DEBUG_MMU,"\nclearing ref for rpte");
            memory[cBigHand] = rpte1 = CLEAR_REF(rpte1);
        } else if (DEFINED(rpte1)) { // if one is non-referenced go to the user page table
            // scout out the upt!! note that this has not been referenced
            if (DEBUG_MMU) { outPTE("\nRPT entry being checked - ", cBigHand); }
            upta = (FRAME(rpte1)<<6);

            for (i = cLittleHand % 64; i < 64;i += 2, cLittleHand = i % 64) { // - iterate over userpage table
                upte1 = memory[upta + (i)];
                if (PINNED(upte1) || FRAME(upte1) == notme) {
                    DEBUG_STATEMENT(DEBUG_MMU,"\nupte1 frame was the notme frame (%x = %x) or pinned", FRAME(upte1), notme);
                    if (DEBUG_MMU) { outPTE("UPT entry being checked - ", upta + i); }

                    continue;
                }

                if (DEFINED(upte1) && REFERENCED(upte1)) { // - if entry is referenced un-reference and move on
                    // clear reference
                    DEBUG_STATEMENT(DEBUG_MMU,"\nclearing ref for upte %d", cLittleHand);
                    memory[cBigHand] = rpte1 = SET_PINNED(rpte1);
                    memory[upta + (i)] = upte1 = CLEAR_REF(upte1);
                } else if (DEFINED(upte1)) { // - otherwise prep it for being put into swap
                    // we can use the frame referenced by upte1
                    if (DEBUG_MMU) { outPTE("UPT entry being checked - ", upta + 1); }
                    DEBUG_STATEMENT(DEBUG_MMU,"\nUpte1 %x, Upte2 %x", upte1, memory[upta + (i + 1)]);
                    memory[cBigHand] = rpte1 = SET_DIRTY(rpte1);
                    frame = FRAME(upte1);
                    swapOutFrame(upta + i);
                    cLittleHand += 2;
                    DEBUG_STATEMENT(DEBUG_MMU,"\nFound a data frame that can be used! (%d, %x)", frame, frame);

                    return frame;
                }
            }

            cLittleHand = 0;
            if (!REFERENCED(rpte1) && !PINNED(rpte1) && FRAME(rpte1) != notme) { // if we only replaced or did nothing to upte entries
                // we can use the frame referenced by rpte1
                frame = FRAME(rpte1);

                DEBUG_STATEMENT(DEBUG_MMU,"\nFound a upt frame that can be used! (%d, %x)", frame, frame);
                swapOutFrame(cBigHand);
                cBigHand += 2;
                return frame;
            } else { // otherwise remove the pin flag
                memory[cBigHand] = rpte1 = CLEAR_PINNED(rpte1);
            }

        }
    } // when you get the bottom start at the top again

    DEBUG_STATEMENT(DEBUG_MMU,"\n No valid frame found notme = %d", notme);
    if (DEBUG_MMU) {  displayTableHierarchy(); }

    return -1;
}

void swapOutFrame(int entry1Index)
{
    // clear the definition bit in entry 1 probably just set entry 1 to 0
    int entry1, entry2;

    entry1 = memory[entry1Index];
    entry2 = memory[entry1Index + 1];

    // if dirty bit is not set and swap exists we are done
    if (DIRTY(entry1) && PAGED(entry2)) {
        DEBUG_STATEMENT(DEBUG_MMU,"\nDirty copy of swap write to old page");
        // if swap exits swap access page with old write
        accessPage(SWAPPAGE(entry2), FRAME(entry1), PAGE_OLD_WRITE);
    } else if (!PAGED(entry2)) {
        DEBUG_STATEMENT(DEBUG_MMU,"\nNo copy in swap write to new swap");
        // if swap doesn't exist then we need to write out to an new write
        memory[entry1Index + 1] = entry2 = SET_PAGED(nextPage);
        accessPage(nextPage, FRAME(entry1), PAGE_NEW_WRITE);
    }

    if (DEBUG_MMU) { outPTE("\nSwapped Entry - (Pre-clear) ", entry1Index); }
    memory[entry1Index] = 0;
    if (DEBUG_MMU) { outPTE("Swapped Entry - ", entry1Index); }

    return;
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
    memAccess += 2;

//	rpta = 0x2400 + RPTI(va);
    rpta = tcb[curTask].RPT + RPTI(va);
	rpte1 = memory[rpta];
	rpte2 = memory[rpta+1];

	// turn off virtual addressing for system RAM
	if (va < 0x3000) return &memory[va];
#if MMU_ENABLE
	if (DEFINED(rpte1))
	{
//        printf("\nUPT frame already defined");
		// defined
        memHits++;
	}
	else
	{
		// fault
        memPageFaults++;
		rptFrame = getFrame(-1);
		rpte1 = SET_DEFINED(rptFrame);
		if (PAGED(rpte2))
		{
            //upt is in swap and we need to read it back in
			accessPage(SWAPPAGE(rpte2), rptFrame, PAGE_READ);
		}
		else
		{
            //initialize the upt memory
            memset(&memory[(rptFrame<<6)], 0, 128);
		}
	}


	memory[rpta] = rpte1 = SET_REF(rpte1);
	memory[rpta+1] = rpte2;

	upta = (FRAME(rpte1)<<6) + UPTI(va);
	upte1 = memory[upta];
	upte2 = memory[upta+1];

	if (DEFINED(upte1))
	{
		// defined
        memHits++;
	}
	else
	{
		// fault
        memPageFaults++;
		uptFrame = getFrame(FRAME(memory[rpta]));
        memory[rpta] = rpte1 = SET_REF(SET_DIRTY(rpte1));
        upte1 = SET_DEFINED(uptFrame);

        if (PAGED(upte2))
		{
            //get the data frame from swap
			accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
		}
		else
		{
            //we don't need to do anything
            //but we could initialize the mem to 0xf025 which is lc-3 halt instruction
            memset(&memory[(uptFrame<<6)], 0xf025, 128);
		}
	}

    if (rwFlg) {
        upte1 = SET_DIRTY(upte1);
    }

    memory[upta] = SET_REF(upte1);
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
		{  fmask = 0x8000;				// move to next word
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
      printf("\nVirtual Memory Space Exceeded!  (%d) - requested nextPage %d, pnum %d", LC3_MAX_PAGE, nextPage, pnum);
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
         memset(&memory[(frame<<6)], 0xf025, 128);
         break;
   }
   return pnum;
} // end accessPage
