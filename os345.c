// os345.c - OS Kernel	09/12/2013
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
#include "os345signals.h"
#include "os345config.h"
#include "os345lc3.h"
#include "os345fat.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static int scheduler(void);
static int dispatcher(void);
int allotTime();

//static void keyboard_isr(void);
//static void timer_isr(void);

int sysKillTask(int taskId);
static int initOS(void);

// **********************************************************************
// **********************************************************************
// global semaphores

Semaphore* semaphoreList;			// linked list of active semaphores

Semaphore* keyboard;				// keyboard semaphore
Semaphore* charReady;				// character has been entered
Semaphore* inBufferReady;			// input buffer ready semaphore

Semaphore* tics1sec;				// 1 second semaphore
Semaphore* tics10sec;				// 10 second semaphore
Semaphore* tics10thsec;				// 1/10 second semaphore

// **********************************************************************
// **********************************************************************
// global system variables

TCB tcb[MAX_TASKS];					// task control block
Semaphore* taskSems[MAX_TASKS];		// task semaphore
jmp_buf k_context;					// context of kernel stack
jmp_buf reset_context;				// context of kernel stack
volatile void* temp;				// temp pointer used in dispatcher

int scheduler_mode;					// scheduler mode
int superMode;						// system mode
int curTask;						// current task #
long swapCount;						// number of re-schedule cycles
char inChar;						// last entered character
int charFlag;						// 0 => buffered input
int inBufIndx;						// input pointer into input buffer
char inBuffer[INBUF_SIZE+1];		// character input buffer
//Message messages[NUM_MESSAGES];		// process message buffers

int deltaTics;						// current clock()
int pollClock;						// current clock()
int lastPollClock;					// last pollClock
bool diskMounted;					// disk has been mounted

time_t oldTime1;					// old 1sec time
time_t oldTime10;					// old 1sec time
clock_t myClkTime;
clock_t myOldClkTime;
PQueue rq;							// ready priority queue
bool schedulerPass = FALSE;


// **********************************************************************
// **********************************************************************
// priority queue functions
int enQ(PQueue queue, TID taskId, Priority priority) {
    if (queue[0] == MAX_TASKS) { // task can't fit into queue || task has completed and been released
    	return -1;
	}

    for (int i = queue[0] + 1; i > 0; i-- ) {
		if (i == 1 || priority > tcb[queue[i - 1]].priority) {
			queue[i] = taskId;
            break;
		}

		queue[i] = queue[i - 1];
	}

    queue[0]++;

    if (DEBUG_QUEUE && (!schedulerPass || !tcb[taskId].name)) {
        printf("\nEnqueue PQueue:");
        for (int i = queue[0]; i > 0; i--) {
            printf("\n\t%s - %d", tcb[queue[i]].name, tcb[queue[i]].priority);
        }
        printf("\nEnd Priority Queue");
    }

    return taskId;
}

int deQ(PQueue queue, TID taskId) {
    TID res = -1;
    if (queue[0] == 0) { // no tasks in queue
		return res;
	}

	if (taskId == -1) {
		res = queue[queue[0]];
	}

	for (int i = queue[0]; i > 0; i--) {
		if (queue[i] == taskId) {
			for (; i < queue[0]; i++) {
				queue[i] = queue[i + 1];
			}
            res = taskId;
		}
	}

    if (res > -1) {
        queue[queue[0]] = NULL;
        queue[0]--;
    }

    if (DEBUG_QUEUE && !schedulerPass) {
        printf("\nDeQ PQueue:");
        for (int i = queue[0]; i > 0; i--) {
            printf("\n\t%s - %d", tcb[queue[i]].name, tcb[queue[i]].priority);
        }
        printf("\nEnd Priority Queue");
    }

    if (res < 0 || !tcb[res].name) { //if task being returned is dead return -1
        return -1;
    }

    return res;
}
// **********************************************************************
// **********************************************************************
// OS startup
//
// 1. Init OS
// 2. Define reset longjmp vector
// 3. Define global system semaphores
// 4. Create CLI task
// 5. Enter scheduling/idle loop
//
int main(int argc, char* argv[])
{
	// save context for restart (a system reset would return here...)
	int resetCode = setjmp(reset_context);
	superMode = TRUE;						// supervisor mode

	switch (resetCode)
	{
		case POWER_DOWN_QUIT:				// quit
			powerDown(0);
			printf("\nGoodbye!!");
			return 0;

		case POWER_DOWN_RESTART:			// restart
			powerDown(resetCode);
			printf("\nRestarting system...\n");

		case POWER_UP:						// startup
			break;

		default:
			printf("\nShutting down due to error %d", resetCode);
			powerDown(resetCode);
			return resetCode;
	}

	// output header message
	printf("%s", STARTUP_MSG);

	// initalize OS
	if ( resetCode = initOS()) return resetCode;

	// create global/system semaphores here
	//?? vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

	charReady = createSemaphore("charReady", BINARY, 0);
	inBufferReady = createSemaphore("inBufferReady", BINARY, 0);
	keyboard = createSemaphore("keyboard", BINARY, 1);
	tics1sec = createSemaphore("tics1sec", BINARY, 0);
	tics10sec = createSemaphore("tics10sec", COUNTING, 0);
	tics10thsec = createSemaphore("tics10thsec", BINARY, 0);

	//?? ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

	// schedule CLI task
	createTask("myShell",			// task name
					P1_shellTask,	// task
					MED_PRIORITY,	// task priority
					argc,			// task arg count
					argv);			// task argument pointers

	// HERE WE GO................

	// Scheduling loop
	// 1. Check for asynchronous events (character inputs, timers, etc.)
	// 2. Choose a ready task to schedule
	// 3. Dispatch task
	// 4. Loop (forever!)

	while(1)									// scheduling loop
	{
		// check for character / timer interrupts
		pollInterrupts();

		// schedule highest priority ready task
		if ((curTask = scheduler()) < 0) continue;

		// dispatch curTask, quit OS if negative return
		if (dispatcher() < 0) break;
	}											// end of scheduling loop

	// exit os
	longjmp(reset_context, POWER_DOWN_QUIT);
	return 0;
} // end main



// **********************************************************************
// **********************************************************************
// scheduler
//
static int scheduler()
{
	int nextTask = -1;
	// ?? Design and implement a scheduler that will select the next highest
	// ?? priority ready task to pass to the system dispatcher.

	// ?? WARNING: You must NEVER call swapTask() from within this function
	// ?? or any function that it calls.  This is because swapping is
	// ?? handled entirely in the swapTask function, which, in turn, may
	// ?? call this function.  (ie. You would create an infinite loop.)

	// ?? Implement a round-robin, preemptive, prioritized scheduler.

	// ?? This code is simply a round-robin scheduler and is just to get
	// ?? you thinking about scheduling.  You must implement code to handle
	// ?? priorities, clean up dead tasks, and handle semaphores appropriately.

	// schedule next task
    schedulerPass = TRUE;
    if (scheduler_mode == 0) {
        // Priority Round Robin
        if ((nextTask = deQ(rq, -1)) >= 0) {
            enQ(rq, nextTask, tcb[nextTask].priority);
        } else {
            nextTask = 0;
        }
    } else {
        // Fair Scheduling
        for (int i = rq[0]; i > 0; i--) { // we need to actually rotate through the rq
            if (tcb[rq[i]].name && tcb[rq[i]].taskTime > 0) {
                nextTask = rq[i];
                --tcb[rq[i]].taskTime;
                break;
            }
        }

        if (nextTask < 0) {
            allotTime();

            nextTask = 0;
        }
    }

    schedulerPass = FALSE;

    if (tcb[nextTask].signal & mySIGSTOP) {
        return -1;
    }

    return nextTask;
} // end scheduler

int allotTime()
{
    // need to recompute fair times and get the next task
    int fair[MAX_TASKS] = {0};
    int privilegeTaken[MAX_TASKS] = {0};
    int time = rq[0] * 10;
    int groupTime, leftOver, i, allotment, privilege, groupId, groups = 0;

    for (i = rq[0]; i > 0; i--) {
        if (rq[i] == 0 || !tcb[rq[i]].name) {
            continue; // skip the shell and dead tasks
        }
        // count number of groups
        // count number of children of each group (in respective group index)
        groupId = !tcb[rq[i]].parent ? rq[i] : tcb[rq[i]].parent;
        if (fair[groupId] == 0) {
            ++groups;
        }
        ++fair[groupId];
    }

    if (!groups) {
        // no groups found
        ++groups;
    }
    groupTime = time / groups; // each group gets an equal amount of time
    leftOver = time - (groupTime * fair[0]); // extra time goes to shell

    for (i = rq[0]; i > 0; i--) {
        if (rq[i] == 0 || !tcb[rq[i]].name) {
            continue; // skip the shell and dead tasks
        }
        groupId = !tcb[rq[i]].parent ? rq[i] : tcb[rq[i]].parent;

        allotment = groupTime/fair[groupId]; // group time / members in the group
        time -= allotment;
        if (!privilegeTaken[groupId]) {
            privilege = groupTime - (allotment * fair[groupId]);
//            printf("\nGroup %d: Members %d, Allotment %d, Privelage %d",groupId, fair[groupId], allotment, privilege);
            allotment += privilege;
            time -= privilege;
            privilegeTaken[groupId] = 1;
        }

        tcb[rq[i]].taskTime = allotment;
    }

    tcb[0].taskTime = time + leftOver; // give all leftover to shell

    return 0;
}

// **********************************************************************
// **********************************************************************
// dispatch curTask
//
static int dispatcher()
{
	int result;

	// schedule task
	switch(tcb[curTask].state)
	{
		case S_NEW:
		{
			// new task
			printf("\nNew Task[%d] %s", curTask, tcb[curTask].name);
			tcb[curTask].state = S_RUNNING;	// set task to run state

			// save kernel context for task SWAP's
			if (setjmp(k_context))
			{
				superMode = TRUE;					// supervisor mode
				break;								// context switch to next task
			}

			// move to new task stack (leave room for return value/address)
			temp = (int*)tcb[curTask].stack + (STACK_SIZE-8);
			SET_STACK(temp);
			superMode = FALSE;						// user mode

			// begin execution of new task, pass argc, argv
			result = (*tcb[curTask].task)(tcb[curTask].argc, tcb[curTask].argv);

			// task has completed
			if (result) printf("\nTask[%d] returned %d", curTask, result);
			else printf("\nTask[%d] returned %d", curTask, result);
			tcb[curTask].state = S_EXIT;			// set task to exit state

			// return to kernal mode
			longjmp(k_context, 1);					// return to kernel
		}

		case S_READY:
		{
			tcb[curTask].state = S_RUNNING;			// set task to run
		}

		case S_RUNNING:
		{
			if (setjmp(k_context))
			{
				// SWAP executed in task
				superMode = TRUE;					// supervisor mode
				break;								// return from task
			}
			if (signals()) break;
			longjmp(tcb[curTask].context, 3); 		// restore task context
		}

		case S_BLOCKED:
		{
			// ?? Could check here to unblock task
			break;
		}

		case S_EXIT:
		{
			if (curTask == 0) return -1;			// if CLI, then quit scheduler
			// release resources and kill task
			sysKillTask(curTask);					// kill current task
			break;
		}

		default:
		{
			printf("Unknown Task[%d] State", curTask);
			longjmp(reset_context, POWER_DOWN_ERROR);
		}
	}
	return 0;
} // end dispatcher



// **********************************************************************
// **********************************************************************
// Do a context switch to next task.

// 1. If scheduling task, return (setjmp returns non-zero value)
// 2. Else, save current task context (setjmp returns zero value)
// 3. Set current task state to READY
// 4. Enter kernel mode (longjmp to k_context)

void swapTask()
{
	assert("SWAP Error" && !superMode);		// assert user mode

	// increment swap cycle counter
	swapCount++;

	// either save current task context or schedule task (return)
	if (setjmp(tcb[curTask].context))
	{
		superMode = FALSE;					// user mode
		return;
	}

	// context switch - move task state to ready
	if (tcb[curTask].state == S_RUNNING) tcb[curTask].state = S_READY;

	// move to kernel mode (reschedule)
	longjmp(k_context, 2);
} // end swapTask



// **********************************************************************
// **********************************************************************
// system utility functions
// **********************************************************************
// **********************************************************************

// **********************************************************************
// **********************************************************************
// initialize operating system
static int initOS()
{
	int i;

	// make any system adjustments (for unblocking keyboard inputs)
	INIT_OS

	// reset system variables
	curTask = 0;						// current task #
	swapCount = 0;						// number of scheduler cycles
	scheduler_mode = 0;					// default scheduler
	inChar = 0;							// last entered character
	charFlag = 0;						// 0 => buffered input
	inBufIndx = 0;						// input pointer into input buffer
	semaphoreList = 0;					// linked list of active semaphores
	diskMounted = 0;					// disk has been mounted

	// malloc ready queue
	rq = (int*)malloc((MAX_TASKS + 1) * sizeof(int));
	if (rq == NULL) return 99;
    rq[0] = 0;

	// capture current time
	lastPollClock = clock();			// last pollClock
	time(&oldTime1);
	time(&oldTime10);

	// init system tcb's
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;				// tcb
		taskSems[i] = NULL;				// task semaphore
	}

	// init tcb
	for (i=0; i<MAX_TASKS; i++)
	{
		tcb[i].name = NULL;
	}

	// initialize lc-3 memory
	initLC3Memory(LC3_MEM_FRAME, 0xF800>>6);

	// ?? initialize all execution queues

	return 0;
} // end initOS



// **********************************************************************
// **********************************************************************
// Causes the system to shut down. Use this for critical errors
void powerDown(int code)
{
	int i;
	printf("\nPowerDown Code %d", code);

	// release all system resources.
	printf("\nRecovering Task Resources...");

	// kill all tasks
	for (i = MAX_TASKS-1; i >= 0; i--)
		if(tcb[i].name) sysKillTask(i);

	// delete all semaphores
	while (semaphoreList)
		deleteSemaphore(&semaphoreList);

	// free ready queue
	free(rq);

	// ?? release any other system resources
	// ?? deltaclock (project 3)

	RESTORE_OS
	return;
} // end powerDown

