// os345p2.c - 5 state scheduling	08/08/2013
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
#include <assert.h>
#include <time.h>
#include "os345.h"
#include "os345signals.h"

#define my_printf	printf

// ***********************************************************************
// project 2 variables
static Semaphore* s1Sem;					// task 1 semaphore
static Semaphore* s2Sem;					// task 2 semaphore

extern TCB tcb[];								// task control block
extern PQueue rq;							// current task #
extern int curTask;							// current task #
extern Semaphore* semaphoreList;			// linked list of active semaphores
extern jmp_buf reset_context;				// context of kernel stack
extern Semaphore* tics10sec;				// context of kernel stack

// ***********************************************************************
// project 2 functions and tasks

int timingTask(int, char**);
int signalTask(int, char**);
int ImAliveTask(int, char**);

// ***********************************************************************
// ***********************************************************************
// project2 command
int P2_project2(int argc, char* argv[])
{
	static char* timeingArgv[] = {"timingTask"};
	static char* s1Argv[] = {"signal1", "s1Sem"};
	static char* s2Argv[] = {"signal2", "s2Sem"};
	static char* aliveArgv[] = {"I'm Alive", "3"};

	printf("\nStarting Project 2");
	SWAP;

	// start tasks looking for sTask semaphores
	createTask("timingTask",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask2",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask3",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask4",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask5",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask6",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask7",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask8",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("timingTask9",				// task name
					timingTask,				// task
			        HIGH_PRIORITY,	// task priority
					1,							// task argc
					timeingArgv);					// task argument pointers

	createTask("signal1",				// task name
					signalTask,				// task
					VERY_HIGH_PRIORITY,	// task priority
					2,							// task argc
					s1Argv);					// task argument pointers

	createTask("signal2",				// task name
					signalTask,				// task
					VERY_HIGH_PRIORITY,	// task priority
					2,							// task argc
					s2Argv);					// task argument pointers

	createTask("I'm Alive",				// task name
					ImAliveTask,			// task
					LOW_PRIORITY,			// task priority
					2,							// task argc
					aliveArgv);				// task argument pointers

	createTask("I'm Alive",				// task name
					ImAliveTask,			// task
					LOW_PRIORITY,			// task priority
					2,							// task argc
					aliveArgv);				// task argument pointers
	return 0;
} // end P2_project2


void P2_printTask(int tid)
{
	printf("\n%4d/%-4d%20s%4d  ", tid, tcb[tid].parent,
		   tcb[tid].name, tcb[tid].priority);
	if (tcb[tid].signal & mySIGSTOP) my_printf("Paused");
	else if (tcb[tid].state == S_NEW) my_printf("New");
	else if (tcb[tid].state == S_READY) my_printf("Ready");
	else if (tcb[tid].state == S_RUNNING) my_printf("Running");
	else if (tcb[tid].state == S_BLOCKED) my_printf("Blocked    %s",
													tcb[tid].event->name);
	else if (tcb[tid].state == S_EXIT) my_printf("Exiting");

	return;
}

// ***********************************************************************
// ***********************************************************************
// list tasks command
int P2_listTasks(int argc, char* argv[])
{
	int i, tid;

	printf("\nReady Queue");
	for (i=rq[0]; i>0; i--)
	{
		tid = rq[i];
		if (tcb[tid].name)
		{
			P2_printTask(tid);
			swapTask();
		}
	}

	Semaphore* sem = semaphoreList;
	while(sem)
	{
		printf("\n\n%s Queue", sem->name);
		for (i = sem->queue[0]; i > 0; i--) {
			tid = sem->queue[i];
			if (tcb[tid].name) {
				P2_printTask(tid);
			}
		}
		sem = (Semaphore*)sem->semLink;
	}

	return 0;
} // end P2_listTasks



// ***********************************************************************
// ***********************************************************************
// list semaphores command
//
int match(char* mask, char* name)
{
   int i,j;

   // look thru name
	i = j = 0;
	if (!mask[0]) return 1;
	while (mask[i] && name[j])
   {
		if (mask[i] == '*') return 1;
		if (mask[i] == '?') ;
		else if ((mask[i] != toupper(name[j])) && (mask[i] != tolower(name[j]))) return 0;
		i++;
		j++;
   }
	if (mask[i] == name[j]) return 1;
   return 0;
} // end match

int P2_listSems(int argc, char* argv[])				// listSemaphores
{
	Semaphore* sem = semaphoreList;
	while(sem)
	{
		if ((argc == 1) || match(argv[1], sem->name))
		{
			printf("\n%20s  %c  %d  %s", sem->name, (sem->type?'C':'B'), sem->state,
	  					tcb[sem->taskNum].name);
		}
		sem = (Semaphore*)sem->semLink;
	}
	return 0;
} // end P2_listSems



// ***********************************************************************
// ***********************************************************************
// reset system
int P2_reset(int argc, char* argv[])						// reset
{
	longjmp(reset_context, POWER_DOWN_RESTART);
	// not necessary as longjmp doesn't return
	return 0;

} // end P2_reset



// ***********************************************************************
// ***********************************************************************
// kill task

int P2_killTask(int argc, char* argv[])			// kill task
{
	int taskId = INTEGER(argv[1]);				// convert argument 1

	if (taskId > 0) printf("\nKill Task %d", taskId);
	else printf("\nKill All Tasks");

	// kill task
	if (killTask(taskId)) printf("\nkillTask Error!");

	return 0;
} // end P2_killTask



// ***********************************************************************
// ***********************************************************************
// signal command
void sem_signal(Semaphore* sem)		// signal
{
	if (sem)
	{
		printf("\nSignal %s", sem->name);
		SEM_SIGNAL(sem);
	}
	else my_printf("\nSemaphore not defined!");
	return;
} // end sem_signal



// ***********************************************************************
int P2_signal1(int argc, char* argv[])		// signal1
{
	if (s1Sem) {
		SEM_SIGNAL(s1Sem);
	}

    return 0;
} // end signal

int P2_signal2(int argc, char* argv[])		// signal2
{
	if (s2Sem) {
		SEM_SIGNAL(s2Sem);
	}

	return 0;
} // end signal



// ***********************************************************************
// ***********************************************************************
// signal task
//
#define COUNT_MAX	5
//
int signalTask(int argc, char* argv[])
{
	int count = 0;					// task variable

	// create a semaphore
	Semaphore** mySem = (!strcmp(argv[1], "s1Sem")) ? &s1Sem : &s2Sem;
	*mySem = createSemaphore(argv[1], 0, 0);

	// loop waiting for semaphore to be signaled
	while(count < COUNT_MAX)
	{
		SEM_WAIT(*mySem);			// wait for signal
		printf("\n%s  Task[%d], count=%d", tcb[curTask].name, curTask, ++count);
	}

    if (!strcmp(argv[1], "s1Sem")) {
        s1Sem = NULL;
    } else {
        s2Sem = NULL;
    }

    return 0;						// terminate task
} // end signalTask



// ***********************************************************************
// ***********************************************************************
// I'm alive task
int ImAliveTask(int argc, char* argv[])
{
	int i;							// local task variable
	while (1)
	{
		printf("\n(%d) I'm Alive!", curTask);
		for (i=0; i<100000; i++) swapTask();
	}
	return 0;						// terminate task
} // end ImAliveTask

int timingTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time

	while (1)
	{
		SEM_WAIT(tics10sec);
		printf("\nCurrent task %d - %s", curTask,myTime(svtime));
        SWAP
	}
	return 0;
}

// **********************************************************************
// **********************************************************************
// read current time
//
char* myTime(char* svtime)
{
	time_t cTime;						// current time

	time(&cTime);						// read current time
	strcpy(svtime, asctime(localtime(&cTime)));
	svtime[strlen(svtime)-1] = 0;		// eliminate nl at end
	return svtime;
} // end myTime
