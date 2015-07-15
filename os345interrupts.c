// os345interrupts.c - pollInterrupts	08/08/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
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
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345config.h"
#include "os345signals.h"

extern TCB tcb[];							// task control block
extern int curTask;							// current task #
extern int deltaTics;					    // deltaTics

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static void keyboard_isr(void);
static void timer_isr(void);

// **********************************************************************
// **********************************************************************
// global semaphores

extern Semaphore* keyboard;				// keyboard semaphore
extern Semaphore* charReady;				// character has been entered
extern Semaphore* inBufferReady;			// input buffer ready semaphore

extern Semaphore* tics1sec;				// 1 second semaphore
extern Semaphore* tics10sec;				// 1 second semaphore
extern Semaphore* tics10thsec;				// 1/10 second semaphore

extern char inChar;				// last entered character
extern int charFlag;				// 0 => buffered input
extern int inBufIndx;				// input pointer into input buffer
extern char inBuffer[INBUF_SIZE+1];	// character input buffer

extern time_t oldTime1;					// old 1sec time
extern time_t oldTime10;				// old 10sec time
extern clock_t myClkTime;
extern clock_t myOldClkTime;

extern int pollClock;				// current clock()
extern int lastPollClock;			// last pollClock

extern int superMode;						// system mode

// **********************************************************************
// **********************************************************************
// Command Line History
char* historyBuffer[HISTORY_SIZE];	// character input buffer
int historyIndex;	// character input buffer
int historyUserIndex;	// character input buffer
bool historyMode;

// **********************************************************************
// **********************************************************************
// simulate asynchronous interrupts by polling events during idle loop
//
void pollInterrupts(void)
{
	// check for task monopoly
	pollClock = clock();
	assert("Timeout" && ((pollClock - lastPollClock) < MAX_CYCLES));
	lastPollClock = pollClock;

	// check for keyboard interrupt
	if ((inChar = GET_CHAR) > 0)
	{
	  keyboard_isr();
	}

	// timer interrupt
	timer_isr();

	return;
} // end pollInterrupts

void beep()
{
	printf("\a");           // beep sound
	fflush(stdout);         // flush buffer so it will sound!
}

void addHistory()
{
	historyMode = FALSE;
	if (historyBuffer[historyIndex] != NULL) {
		free(historyBuffer[historyIndex]);
	}
	historyBuffer[historyIndex] = (char*) malloc((strlen(inBuffer) + 1) * sizeof(char));
	strcpy(historyBuffer[historyIndex], inBuffer);
	historyIndex = (historyIndex + 1) % HISTORY_SIZE;
	historyUserIndex = historyIndex;
}

void clearLine()
{
	while (inBufIndx) {
		printf("\b \b");
		inBufIndx--;
	}
}

void loadHistory()
{
	if (historyMode) {
		clearLine();
		strcpy(inBuffer, historyBuffer[historyUserIndex]);
		inBufIndx = strlen(inBuffer);
		printf(historyBuffer[historyUserIndex]);
	}
}
// **********************************************************************
// keyboard interrupt service routine
//
static void keyboard_isr()
{
	// assert system mode
	assert("keyboard_isr Error" && superMode);

	semSignal(charReady);					// SIGNAL(charReady) (No Swap)
	if (charFlag == 0)
	{
		switch (inChar)
		{
			case '\r':
			case '\n':
			{
				if (DEBUG_PARSER) {
					printf("\nEOL detected");
				}

				addHistory();
				inBufIndx = 0;				// EOL, signal line ready
				semSignal(inBufferReady);	// SIGNAL(inBufferReady)
				break;
			}

			case 0x12:						// ^r
			{
				if (DEBUG_PARSER) {
					printf("\nCntrl - R");
				}
				inBufIndx = 0;
				inBuffer[0] = 0;
				sigSignal(-1, mySIGCONT);		// all tasks continue
				for (int taskId=0; taskId<MAX_TASKS; taskId++)
				{
					tcb[taskId].signal &= (~mySIGSTOP & ~mySIGTSTP);
				}
				break;
			}

			case 0x17:						// ^w
			{
				if (DEBUG_PARSER) {
					printf("\nCntrl - W");
				}
				inBufIndx = 0;
				inBuffer[0] = 0;
				sigSignal(-1, mySIGSTOP);		// all tasks stop

				break;
			}

			case 0x18:						// ^x
			{
				if (DEBUG_PARSER) {
					printf("\nCntrl - X");
				}
				inBufIndx = 0;
				inBuffer[0] = 0;
				sigSignal(0, mySIGINT);		// interrupt task 0
				semSignal(inBufferReady);	// SEM_SIGNAL(inBufferReady)
				break;
			}

            case 0x7f:                      // backspace signal
            {
				if (DEBUG_PARSER) {
					printf("\nBackspace detected");
				}
                if(inBufIndx > 0)
                {
                    inBufIndx--;
                    inBuffer[inBufIndx] = 0;
                    printf("\b \b");
                } else {
                    beep();
                }

                break;
            }

			case 0x1B:
			{ //an arrow key has been pressed
				if (DEBUG_PARSER) {
					printf("\nArrow key detected");
				}
				getchar();
				int arrowId = getchar();

				switch (arrowId) {
					case 65: //up arrow pressed
					{
//						printf("\nPressed Up");
						int temp = (historyUserIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;
						int blockIndex = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE;
						if ((!historyMode || temp != blockIndex)  && historyBuffer[temp] != NULL) {
							historyUserIndex = temp;
						} else { // reached the top of the history
							beep();
						}

						historyMode = TRUE;

						break;
					}
					case 66: //down arrow pressed
					{
						if (!historyMode) {
							beep();
						} else {
							int temp = (historyUserIndex + 1) % HISTORY_SIZE;
							if (temp == historyIndex) {
								clearLine();
								historyMode = FALSE;
								inBuffer[0] = 0;
								historyUserIndex = historyIndex;
								break;
							}
							historyUserIndex = temp;
						}
						break;
					}
					case 67: //right arrow pressed
					{
//						printf("\nPressed Right");
						break;
					}
					case 68: //left arrow pressed
					{
//						printf("\nPressed Left");
						break;
					}
				}

				if (DEBUG_PARSER) {
					printf("\nHistory Index: %d, History User Index: %d", historyIndex, historyUserIndex);
				}

				//load buffer into history
				loadHistory();

				break;
			}

            default:
			{
				if (DEBUG_PARSER) {
					printf("\nCharacter detected. dec: %d hex: 0x%x", inChar, inChar);
				}
				if (inBufIndx < INBUF_SIZE) {
					inBuffer[inBufIndx++] = inChar;
					inBuffer[inBufIndx] = 0;
					printf("%c", inChar);        // echo character

				} else {
					beep();
				}
			}
		}
	}
	else
	{
		// single character mode
		inBufIndx = 0;
		inBuffer[inBufIndx] = 0;
	}
	return;
} // end keyboard_isr


// **********************************************************************
// timer interrupt service routine
//
static void timer_isr()
{
	time_t currentTime;						// current time

	// assert system mode
	assert("timer_isr Error" && superMode);

	// capture current time
  	time(&currentTime);

  	// one second timer
  	if ((currentTime - oldTime1) >= 1)
  	{
		// signal 1 second
  	   semSignal(tics1sec);
		oldTime1 += 1;
  	}

	// sample fine clock
	myClkTime = clock();
	if ((myClkTime - myOldClkTime) >= ONE_TENTH_SEC)
	{
        deltaTics++;
		myOldClkTime = myOldClkTime + ONE_TENTH_SEC;   // update old
		semSignal(tics10thsec);
        updateDeltaClock(deltaTics);
	}

	if ((currentTime - oldTime10) >= 3) {
		semSignal(tics10sec);
		oldTime10 += 3;
	}

	return;
} // end timer_isr
