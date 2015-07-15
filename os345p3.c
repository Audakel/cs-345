// os345p3.c - Jurassic Park
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"

// ***********************************************************************
// project 3 variables

// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over
extern TCB tcb[];
extern int deltaTics;

Delta* deltaClock[MAX_DELTAS];
int deltaCount;
int timeTaskID;
Semaphore* dcChange;
Semaphore* dcMutex;
// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);
int timeTask(int argc, char* argv[]);
int dcMonitorTask(int argc, char* argv[]);
void printDeltaClock(void);


// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char* newArgv[2];

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

    dcChange = createSemaphore("dcChange",0,1);
    dcMutex = createSemaphore("dcMutex",0,1);

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	//?? create car, driver, and visitor tasks here

	return 0;
} // end project3



// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printDeltaClock();

	return 0;
} // end CL3_dc

// **********************************************************************
// **********************************************************************
// delta clock functions
int insertDeltaClock(int time, Semaphore *sem)
{
    if (deltaCount >= MAX_DELTAS) { // couldn't insert because deltaClock was full
        return -1;
    }

    SEM_WAIT(dcMutex)
    Delta* delta = (Delta*) malloc(sizeof(Delta)); SWAP
    delta->time = time; SWAP
    delta->sem = sem; SWAP

    for (int i = deltaCount; i > -1; i-- ) {
        if (i == 0 || delta->time < deltaClock[i - 1]->time) {
            deltaClock[i] = delta; SWAP
            if (i > 0) { // adjust the previous delta
                deltaClock[i - 1]->time -= delta->time; SWAP
            }
            break;
        }

        delta->time -= deltaClock[i - 1]->time; SWAP
        deltaClock[i] = deltaClock[i - 1]; SWAP
    }

    deltaCount++; SWAP


    if (DEBUG_DELTA_CLOCK) {
        printf("\nInserted Delta into Delta Clock: %d", time);
        printDeltaClock();
        printf("\nEnd Delta Clock");
    }

    SEM_SIGNAL(dcMutex)
    return 0;
}

// **********************************************************************
// **********************************************************************
// delta clock update
int updateDeltaClock(int tics)
{
    if (!dcChange) {
        return 0;
    }

    if (deltaCount == 0) {
        deltaTics = 0;
        return 0;
    }

    if (dcMutex->state && deltaCount > 0) {
        int afterTics = tics - deltaClock[deltaCount - 1]->time;
        deltaClock[deltaCount - 1]->time -= tics;

        while (deltaCount > 0 && deltaClock[deltaCount - 1]->time <= 0) { // we need to remove from the list
            SEM_SIGNAL(deltaClock[deltaCount - 1]->sem)
            free(deltaClock[deltaCount - 1]);
            deltaClock[deltaCount - 1] = NULL;
            deltaCount--;

            if (afterTics > 0 && deltaCount > 0) {
                int temp = afterTics - deltaClock[deltaCount - 1]->time;
                deltaClock[deltaCount - 1]->time -= afterTics;
                afterTics = temp;
            }
            SEM_SIGNAL(dcChange)
        }

        deltaTics = 0;
    }

    return 0;
}

// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void)
{
    int i;
    printf("\nDeltas in clock: %d", deltaCount);
    for (i=deltaCount - 1; i > -1; i--)
	{
        printf("\n%4d%4d  %-20s", deltaCount - i, deltaClock[i]->time, deltaClock[i]->sem->name);
    }
    return;
}


// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[])
{
    dcChange = createSemaphore("dcChange",0,1);
    dcMutex = createSemaphore("dcMutex",0,1);
    deltaCount = 0;

    printf("\nCreating DC Test Task");
    createTask( "DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

    printf("\nCreating Time Task");
    timeTaskID = createTask( "Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
    printf("\nComplete");
	return 0;
} // end P3_tdc



// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	// create some test times for event[0-9]
    Semaphore* event[10];
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};
    // 40, 50, 50, 90, 110, 170, 300, 300, 300, 340
    // 40, 10 , 0, 40, 20, 60, 130, 0, 0, 40

	for (i=0; i<10; i++)
	{
        sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
        insertDeltaClock(ttime[i], event[i]);
	}
	printDeltaClock();

	while (deltaCount > 0)
	{
        SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
            if (event[i]->state ==1)
            {
                printf("\n  event[%d] signaled", i);
                event[i]->state = 0;
                flg = 1;
			}
		}
		if (flg) printDeltaClock();
	}
	printf("\nNo more events in Delta Clock");

	// kill dcMonitorTask
	tcb[timeTaskID].state = S_EXIT;
	return 0;
} // end dcMonitorTask


extern Semaphore* tics1sec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	while (1)
	{
		SEM_WAIT(tics1sec)
		printf("\nTime = %s", myTime(svtime));
	}
	return 0;
} // end timeTask


