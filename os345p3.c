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
int gDriverId;
Semaphore* dcChange;
Semaphore* dcMutex;
Semaphore* tickets;
// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);
int timeTask(int argc, char* argv[]);
int dcMonitorTask(int argc, char* argv[]);
int carTask(int argc, char* argv[]);
int visitorTask(int argc, char* argv[]);
int driverTask(int argc, char* argv[]);
void printDeltaClock(void);
void randomizeVisitor(int max, Semaphore* sem);
void moveToTicketLine();
void addVisitorToParkLine();
void reduceTicketsAvailable();
void waitForTicket(Semaphore* visitorWait);
void moveToMuseumLine();
void moveToMuseum();
void moveToTourLine();
void moveToTour();
Semaphore* getPassegnerWaitSem();
void passPassengerWaitSem(Semaphore *sem);
Semaphore* getDriverWaitSem(int carId);
void passDriverWaitSem(Semaphore* sem, int driverId);
void waitForMuseumExperience(Semaphore* visitorWait);
void waitForTourExperience(Semaphore* visitorWait);
void moveToGiftShopLine();
void waitForGiftShopExperience(Semaphore* visitorWait);
void exitPark();
void moveVisitorToCar(int myId);
void moveVisitorOutOfCar(int myId);
void updateCarDriver(int carId, int driverId);

// ***********************************************************************
// semaphores
Semaphore* getPassenger;
Semaphore* seatTaken;
Semaphore* needDriver;
Semaphore* wakeupDriver;
Semaphore* roomInPark;
Semaphore* driverReady;
Semaphore* needTicket;
Semaphore* ticketReady;
Semaphore* buyTicket;
Semaphore* needPassengerWait;
Semaphore* passengerResourceReady;
Semaphore* driverResourceReady;
Semaphore* gSemaphore;
Semaphore* needDriverSem;
Semaphore* passengerResourceAcquired;
Semaphore* driverResourceAcquired;
Semaphore* resourceMutex;
Semaphore* getTicketMutex;
Semaphore* roomInMuseum;
Semaphore* roomInGiftShop;
Semaphore* getDriverMutex;

// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char idBuf[32];
	char* newArgv[2];
    int i;

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

    tickets = createSemaphore("tickets", COUNTING, MAX_TICKETS);
    roomInPark = createSemaphore("roomInPark",COUNTING, MAX_IN_PARK);
    roomInMuseum = createSemaphore("roomInMuseum",COUNTING, MAX_IN_MUSEUM);
    roomInGiftShop = createSemaphore("roomInGiftShop",COUNTING, MAX_IN_GIFTSHOP);
    wakeupDriver = createSemaphore("wakeupDriver",COUNTING,0);

    dcChange = createSemaphore("dcChange",BINARY,1);
    dcMutex = createSemaphore("dcMutex",BINARY,1);
    resourceMutex = createSemaphore("resourceMutex",BINARY,1);
    getTicketMutex = createSemaphore("getTicketMutex",BINARY,1);
    getDriverMutex = createSemaphore("getDriverMutex",BINARY,1);

    getPassenger = createSemaphore("getPassenger",BINARY,0);
    seatTaken = createSemaphore("seatTaken",BINARY,0);
    needDriver = createSemaphore("needDriver",BINARY,0);
    driverReady = createSemaphore("driverReady",BINARY,0);
    needTicket = createSemaphore("needTicket",BINARY,0);
    ticketReady = createSemaphore("ticketReady",BINARY,0);
    buyTicket = createSemaphore("buyTicket",BINARY,0);
    needPassengerWait = createSemaphore("needPassengerWait",BINARY,0);
    passengerResourceReady = createSemaphore("passengerResourceReady",BINARY,0);
    driverResourceReady = createSemaphore("driverResourceReady",BINARY,0);
    needDriverSem = createSemaphore("needDriverSem",BINARY,0);
    passengerResourceAcquired = createSemaphore("passengerResourceAcquired",BINARY,0);
    driverResourceAcquired = createSemaphore("driverResourceAcquired",BINARY,0);


    // wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");

	// create car tasks
    for (i = 0; i < NUM_CARS; i++) {
        sprintf(buf, "car%d", i);	SWAP;
        sprintf(idBuf, "%d", i);	SWAP;
        newArgv[0] = buf;
        newArgv[1] = idBuf;
        createTask(buf,
                   carTask,
                   MED_PRIORITY,
                   2,
                   newArgv);		SWAP;
    }

    // create driver tasks
    for (i = 0; i < NUM_DRIVERS; i++) {
        sprintf(buf, "driver%d", i);	SWAP;
        sprintf(idBuf, "%d", i);		SWAP;
        newArgv[0] = buf;
        newArgv[1] = idBuf;
        createTask(buf,
                   driverTask,
                   MED_PRIORITY,
                   2,
                   newArgv);		SWAP;
    }

    // create visitor tasks
    for (i = 0; i < NUM_VISITORS; i++) {
        sprintf(buf, "visitor%d", i);	SWAP;
        sprintf(idBuf, "%d", i);		SWAP;
        newArgv[0] = buf;
        newArgv[1] = idBuf;
        createTask(buf,
                   visitorTask,
                   MED_PRIORITY,
                   2,
                   newArgv);		SWAP;
    }

    return 0;
} // end project3

int carTask(int argc, char* argv[])
{

    char buf[32];           		                SWAP;
    int myID = atoi(argv[1]);		                SWAP;	// get unique drive id
    int i;                                          SWAP;
    sprintf(buf,"%sTourOver", argv[0]);
    Semaphore* passengerWait[NUM_SEATS];
    Semaphore* driverDone;
    printf("Starting carTask%d", myID);		        SWAP;

    while (1) {
        for (i = 0; i < NUM_SEATS; i++) {
            SEM_WAIT(fillSeat[myID]);               SWAP;

            SEM_SIGNAL(getPassenger);               SWAP; // signal for visitor

            SEM_WAIT(seatTaken);                    SWAP; // wait for visitor to reply

            passengerWait[i] = getPassegnerWaitSem(); SWAP;// save passenger ride over semaphore
//            printf("\nGot passenger");
            SEM_SIGNAL(seatFilled[myID]);            SWAP; // signal visitor in seat
//            printf("\nPassenger filled seat");
        }

//        printf("\nPassengers full");
        SEM_WAIT(getDriverMutex);
        {
//            printf("\n%s Got getDriverMutex", argv[0]);
//            printf("\n%s needs a driver", argv[0]);
            SEM_SIGNAL(needDriver);                 SWAP;
//            printf("\n%s needDriver state: %d", argv[0], needDriver->state);
            // wakeup attendant
            SEM_SIGNAL(wakeupDriver);               SWAP;

            // save driver ride over semaphore
            driverDone = getDriverWaitSem(myID);        SWAP;
//            printf("\n%s got a driver", argv[0]);
        }
        SEM_SIGNAL(getDriverMutex);
//        printf("\n%s released getDriverMutex", argv[0]);

//        printf("\n%s - SEM_WAIT(rideOver[%d])", argv[0], myID);
        SEM_WAIT(rideOver[myID]);                   SWAP;
//        printf("\n%s tour is done", argv[0]);

        SEM_SIGNAL(driverDone);                     SWAP;
//        printf("\n%s released Driver", argv[0]);

        for (i = 0; i < NUM_SEATS; i++)
        {
//            printf("\n%s released passenger [%d]", argv[0], i);
            SEM_SIGNAL(passengerWait[i]);           SWAP;
        }

        if (myPark.numExitedPark == NUM_VISITORS) {
            break;
        }
    }

    return 0;
}

// ***********************************************************************
// ***********************************************************************
// Visitor schedule
// randomly insert into park line (10 sec max wait)
// go in the park (only MAX_IN_PARK are allowed)
// randomly insert into ticket line (3 sec max wait)
// get ticket (only MAX_TICKETS allowed to be in park)
// randomly insert into the museum line (3 sec max wait)
// go into museum (only MAX_IN_MUSEUM allowed in museum)
// random wait in museum
// go to tourLine
// wait to get on tour car
// exchange rideDone semaphore with car
// give back ticket to the park
// randomly insert into the gift shop line (3 sec max wait)
// random time in gift shop (only MAX_IN_GIFTSHOP allowed)
// exit the park
int visitorTask(int argc, char* argv[])
{

    char buf[32];
    int visitorId = atoi(argv[1]);
    sprintf(buf,"%sWait",argv[0]);
    Semaphore* visitorWait = createSemaphore(buf,BINARY,0);

//    printf("\n%s is arriving at park line", argv[0]);
    randomizeVisitor(100, visitorWait);

//    printf("\n%s got in park line", argv[0]);
    addVisitorToParkLine();

//    printf("\n%s is waiting to get in", argv[0]);
    SEM_WAIT(roomInPark);

//    printf("\n%s is getting in ticket line", argv[0]);
    moveToTicketLine();

//    printf("\n%s is waiting for ticket", argv[0]);
    waitForTicket(visitorWait);

//    printf("\n%s is getting in museum line", argv[0]);
    moveToMuseumLine();

//    printf("\n%s is waiting for museum tour", argv[0]);
    waitForMuseumExperience(visitorWait);

//    printf("\n%s is getting in car line", argv[0]);
    moveToTourLine();

//    printf("\n%s is waiting for tour", argv[0]);
    waitForTourExperience(visitorWait);

//    printf("\n%s is waiting for gift shop line", argv[0]);
    moveToGiftShopLine();

//    printf("\n%s is buying gifts", argv[0]);
    waitForGiftShopExperience(visitorWait);

//    printf("\n%s left park", argv[0]);
    exitPark();

    SEM_SIGNAL(roomInPark);

    return 0;
}

int driverTask(int argc, char* argv[])
{
    char buf[32];
    Semaphore* driverDone;
    int myID = atoi(argv[1]);		            SWAP;	// get unique drive id
    printf(buf, "Starting driverTask%d", myID);		SWAP;
    sprintf(buf, "driverDone%d", myID); 		    SWAP;
    driverDone = createSemaphore(buf, BINARY, 0);	SWAP; 	// create notification event

    while(1)				// such is my life!!
    {
        SEM_WAIT(wakeupDriver);		                SWAP;	// goto sleep
//        printf("\nDriver %s woke up", argv[0]);
        if (SEM_TRYLOCK(needDriver))			// i’m awake  - driver needed?
        {
//            printf("\n%s someone needs a driver", argv[0]);
            passDriverWaitSem(driverDone, myID);  		SWAP;	// pass notification semaphore
//            printf("\n%s passed driver sem", argv[0]);
            SEM_SIGNAL(driverReady);		        SWAP;	// driver is awake
//            printf("\n%s is driving car", argv[0]);
            SEM_WAIT(driverDone);		            SWAP;	// drive ride

            updateCarDriver(myID, 0);               SWAP;
//            printf("\n%s tour is done", argv[0]);
//            printf("\n%s is going back to sleep", argv[0]);
        }
        else if (SEM_TRYLOCK(needTicket))			// someone need ticket?
        {					// yes
            updateCarDriver(myID, -1);               SWAP;
//            printf("\n%s someone needs a ticket", argv[0]);
            SEM_WAIT(tickets);		                SWAP;	// wait for ticket (counting)
//            printf("\n%s sees ticket is available", argv[0]);
            SEM_SIGNAL(ticketReady);		        SWAP;	// print a ticket (binary)
//            printf("\n%s handed ticket to customer", argv[0]);
            SEM_WAIT(buyTicket);                    SWAP;	// wait for ticket to be bought
            updateCarDriver(myID, 0);
//            printf("\n%s saw customer take ticket", argv[0]);
//            printf("\n%s is going back to sleep", argv[0]);
        }
        else if (myPark.numExitedPark == NUM_VISITORS) {
//            printf("\nDriver %s is done for the day no more visitors", argv[0]);
            break;
        } else {
//            printf("\nDriver %s woke up for no reason", argv[0]);
        }
    }
    return 0;

}

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

    SEM_WAIT(dcMutex);
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

    SEM_SIGNAL(dcMutex);
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
            SEM_SIGNAL(deltaClock[deltaCount - 1]->sem);
            free(deltaClock[deltaCount - 1]);
            deltaClock[deltaCount - 1] = NULL;
            deltaCount--;

            if (afterTics > 0 && deltaCount > 0) {
                int temp = afterTics - deltaClock[deltaCount - 1]->time;
                deltaClock[deltaCount - 1]->time -= afterTics;
                afterTics = temp;
            }
            SEM_SIGNAL(dcChange);
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
        SEM_WAIT(dcChange);
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
		SEM_WAIT(tics1sec);
		printf("\nTime = %s", myTime(svtime));
	}
	return 0;
} // end timeTask

void randomizeVisitor(int max, Semaphore* sem)
{
    insertDeltaClock(rand() % max, sem);

    SEM_WAIT(sem);
}

void addVisitorToParkLine()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numOutsidePark++;        SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToTicketLine()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numOutsidePark--;        SWAP;
        myPark.numInPark++;             SWAP;
        myPark.numInTicketLine++;        SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToMuseumLine()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numInTicketLine--;        SWAP;
        myPark.numInMuseumLine++;             SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToMuseum()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numInMuseumLine--;        SWAP;
        myPark.numInMuseum++;             SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToTourLine()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numInMuseum--;        SWAP;
        myPark.numInCarLine++;             SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToTour()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numInCarLine--;          SWAP;
        myPark.numInCars++;             SWAP;
        myPark.numTicketsAvailable++;   SWAP;

        SEM_SIGNAL(tickets);
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToGiftShopLine()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
//        printf("\nMoving to gift shop line");
        // access inside park variables
        myPark.numInCars--;             SWAP;
        myPark.numInGiftLine++;         SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveToGiftShop()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numInGiftLine--;         SWAP;
        myPark.numInGiftShop++;         SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void exitPark()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numInPark--;             SWAP;
        myPark.numInGiftShop--;         SWAP;
        myPark.numExitedPark++;         SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void reduceTicketsAvailable()
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.numTicketsAvailable--;   SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveVisitorToCar(int myId)
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.cars[myId].passengers++; SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void moveVisitorOutOfCar(int myId)
{
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
        myPark.cars[myId].passengers--; SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

void updateCarDriver(int driverId, int position)
{
//    char driver[] = {'T', 'z', 'A', 'B', 'C', 'D' };
    // protect shared memory access
    SEM_WAIT(parkMutex);                SWAP;
    {
        // access inside park variables
//        printf("\n%d status set to %c", driverId, driver[position + 1]);
        myPark.drivers[driverId] = position; SWAP;
    }
    // release protect shared memory access
    SEM_SIGNAL(parkMutex);              SWAP;
}

Semaphore* getPassegnerWaitSem()
{
    Semaphore* waitSem;
//    printf("\nNeed passengers semaphore");
    SEM_SIGNAL(needPassengerWait);          SWAP;
    SEM_WAIT(passengerResourceReady);       SWAP;
//    printf("\nPassenger Semaphore ready");
    waitSem = gSemaphore;                   SWAP;
    SEM_SIGNAL(passengerResourceAcquired);  SWAP;
//    printf("\nGot passenger semaphore");

    return waitSem;
}

void passPassengerWaitSem(Semaphore *sem)
{
//    printf("\nPassing Passenger Sem");
    SEM_WAIT(resourceMutex);          SWAP;
    SEM_SIGNAL(seatTaken);            SWAP;
    SEM_WAIT(needPassengerWait);      SWAP;
//    printf("\nPassegnger Sem was asked for");
    gSemaphore = sem;                 SWAP;
//    printf("\nPrepared Passenger Sem");
    SEM_SIGNAL(passengerResourceReady);        SWAP;
    SEM_WAIT(passengerResourceAcquired);       SWAP;
//    printf("\nThey got the passenger sem");
    SEM_SIGNAL(resourceMutex);        SWAP;
//    printf("\nSuccessfully passed sem");
}

Semaphore* getDriverWaitSem(int carId)
{
    Semaphore* waitSem;
    SEM_SIGNAL(needDriverSem); SWAP;
    SEM_WAIT(driverResourceReady); SWAP;
    waitSem = gSemaphore; SWAP;
    updateCarDriver(gDriverId, carId + 1); SWAP;
    SEM_SIGNAL(driverResourceAcquired); SWAP;
//    printf("\nDrive sem acquired");

    return waitSem;
}

void passDriverWaitSem(Semaphore *sem, int driverId)
{
    SEM_WAIT(resourceMutex); SWAP;
    SEM_WAIT(needDriverSem); SWAP;
    gSemaphore = sem; SWAP;
    gDriverId = driverId; SWAP;
    SEM_SIGNAL(driverResourceReady); SWAP;
    SEM_WAIT(driverResourceAcquired); SWAP;
//    printf("\nThey got driver sem");
    SEM_SIGNAL(resourceMutex); SWAP;
}

void waitForTicket(Semaphore* visitorWait)
{
    randomizeVisitor(30, visitorWait);
    // wait for you to be at the front of the line
    SEM_WAIT(getTicketMutex);           SWAP;
    {
        // signal need ticket (produce raised hand up)
        SEM_SIGNAL(needTicket);		    SWAP;

        // wakeup driver (produce wakeup call, driver consumes)
        SEM_SIGNAL(wakeupDriver);		SWAP;

        // wait ticket available (consume, driver produced ticket)
        SEM_WAIT(ticketReady);		    SWAP;

        // buy ticket (produce driver consumes notification)
        SEM_SIGNAL(buyTicket);		    SWAP;

        reduceTicketsAvailable();
    }
    // allow next visitor to purchase ticket
    SEM_SIGNAL(getTicketMutex);         SWAP;
}

void waitForMuseumExperience(Semaphore* visitorWait)
{
    randomizeVisitor(30, visitorWait);

    // wait for you to be at the front of the line
    SEM_WAIT(roomInMuseum);           SWAP;
    {
        moveToMuseum();

        randomizeVisitor(30, visitorWait);
    }
    // allow next visitor to purchase ticket
    SEM_SIGNAL(roomInMuseum);         SWAP;
}

void waitForTourExperience(Semaphore* visitorWait)
{
    // wait for you to be at the front of the line
    randomizeVisitor(30, visitorWait);

    SEM_WAIT(getPassenger);             SWAP;
//    printf("\nTour car available");

    moveToTour();                       SWAP;
//    printf("\nGot on car");
    passPassengerWaitSem(visitorWait);  SWAP;
//    printf("\nPassed visitor wait sem");

//    printf("\nSeat belt buckled");

    SEM_WAIT(visitorWait);              SWAP;
//    printf("\nTour ended");
}

void waitForGiftShopExperience(Semaphore* visitorWait)
{
    randomizeVisitor(30, visitorWait);
    // wait for you to be at the front of the line
    SEM_WAIT(roomInGiftShop);           SWAP;

    moveToGiftShop();

    randomizeVisitor(30, visitorWait);

    SEM_SIGNAL(roomInGiftShop);
}