// os345p2.c - Multi-tasking
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
#include "os345.h"

#define my_printf	printf

// ***********************************************************************
// project 2 variables
static Semaphore* s1Sem;					// task 1 semaphore
static Semaphore* s2Sem;					// task 2 semaphore

extern TCB tcb[];								// task control block
extern int curTask;							// current task #
extern Semaphore* semaphoreList;			// linked list of active semaphores
extern jmp_buf reset_context;				// context of kernel stack

// ***********************************************************************
// project 2 functions and tasks

int signalTask(int, char**);
int ImAliveTask(int, char**);
int timerTask(int argc, char* argv[]);

// ***********************************************************************
// added functionality as part of project 2
// P2_enQ	-	place task in passed-in queue
int P2_enQ(int* queue, int qNumTasks, int taskID)
{
	int i;
	int loc = 0;
	for(i = qNumTasks; i > 0; i--)
	{
		if (tcb[queue[i-1]].priority >= tcb[taskID].priority)
		{
			loc = i;
			break;
		}
		else
		{
			queue[i] = queue[i-1];
		}
	}
	queue[loc] = taskID;

	return 0;
} // end P2_enQ

// P2_unQ	-	remove task from passed in queue
int P2_unQ(int* queue, int qNumTasks, int taskID)
{
	int i;
	int loc = -2;

	if (qNumTasks == 0)
		return -2;		// no task in queue, error

	for (i = 0; i < qNumTasks; i++)
	{
		if (queue[i] == taskID)
		{
			loc = i;
			break;
		}
	}

	if(loc == -2)
		return -2;		// not found in queue, error

	for (i = loc; i < qNumTasks; i++)
	{
		queue[i] = queue[i+1];
	}

	return 0;
} // end P2_deQ




// ***********************************************************************
// ***********************************************************************
// project2 command
int P2_project2(int argc, char* argv[])
{
	static char* s1Argv[] = {"signal1", "s1Sem"};
	static char* s2Argv[] = {"signal2", "s2Sem"};
	static char* aliveArgv[] = {"I'm Alive", "3"};
	static char* timer1Argv[] = {"Timer1"};
	static char* timer2Argv[] = {"Timer2"};
	static char* timer3Argv[] = {"Timer3"};
	static char* timer4Argv[] = {"Timer4"};
	static char* timer5Argv[] = {"Timer5"};
	static char* timer6Argv[] = {"Timer6"};
	static char* timer7Argv[] = {"Timer7"};
	static char* timer8Argv[] = {"Timer8"};
	static char* timer9Argv[] = {"Timer9"};

	printf("\nStarting Project 2");
	SWAP;

	// start timer tasks
	createTask(timer1Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer1Argv);
	createTask(timer2Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer2Argv);
	createTask(timer3Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer3Argv);
	createTask(timer4Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer4Argv);
	createTask(timer5Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer5Argv);
	createTask(timer6Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer6Argv);
	createTask(timer7Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer7Argv);
	createTask(timer8Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer8Argv);
	createTask(timer9Argv[0],
					timerTask,
					HIGH_PRIORITY,
					1,
					timer9Argv);

	// start tasks looking for sTask semaphores
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



// ***********************************************************************
// ***********************************************************************
// list tasks command
int P2_listTasks(int argc, char* argv[])
{
	int i,j;
	extern int* rq;
	extern int numTasks;
	Semaphore* sem = semaphoreList;

//	?? 1) List all tasks in all queues
// ?? 2) Show the task stake (new, running, blocked, ready)
// ?? 3) If blocked, indicate which semaphore
	if(numTasks > 0)
	{
		printf("\nReadyQueue");
		for (j = 0; j < numTasks; j++)
		{
			i = rq[j];
			printf("\n    tid%2d/pid%2d%15s%4d  ", i, tcb[i].parent,
		  				tcb[i].name, tcb[i].priority);
			if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
			else if (tcb[i].state == S_NEW) my_printf("New");
			else if (tcb[i].state == S_READY) my_printf("Ready");
			else if (tcb[i].state == S_RUNNING) my_printf("Running");
			else if (tcb[i].state == S_BLOCKED) my_printf("Blocked    %s",
		  				tcb[i].event->name);
			else if (tcb[i].state == S_EXIT) my_printf("Exiting");
			swapTask();
		}
	}

	// go through all the semaphores and print out what's blocked on them
	while(sem)
	{
		if(sem->numBlocked > 0)
		{
			printf("\nBlocked Queue of %s", sem->name);
			//printf("\n%s  %c  %d  %s", sem->name, (sem->type?'C':'B'), sem->state,
	  		//			tcb[sem->taskNum].name);

			for (j = 0; j < sem->numBlocked; j++)
			{
				i = sem->blockedQ[j];
				printf("\n    tid%2d/pid%2d%15s%4d  ", i, tcb[i].parent,
			  				tcb[i].name, tcb[i].priority);
				if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
				else if (tcb[i].state == S_NEW) my_printf("New");
				else if (tcb[i].state == S_READY) my_printf("Ready");
				else if (tcb[i].state == S_RUNNING) my_printf("Running");
				else if (tcb[i].state == S_BLOCKED) my_printf("Blocked    %s",
			  				tcb[i].event->name);
				else if (tcb[i].state == S_EXIT) my_printf("Exiting");
				swapTask();
			}
		}
		
		sem = (Semaphore*)sem->semLink;
	}
/*
	for (i=0; i<MAX_TASKS; i++)
	{
		if (tcb[i].name)
		{
			printf("\n%4d/%-4d%20s%4d  ", i, tcb[i].parent,
		  				tcb[i].name, tcb[i].priority);
			if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
			else if (tcb[i].state == S_NEW) my_printf("New");
			else if (tcb[i].state == S_READY) my_printf("Ready");
			else if (tcb[i].state == S_RUNNING) my_printf("Running");
			else if (tcb[i].state == S_BLOCKED) my_printf("Blocked    %s",
		  				tcb[i].event->name);
			else if (tcb[i].state == S_EXIT) my_printf("Exiting");
			swapTask();
		}
	}
*/
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
	int taskId = INTEGER(argv[1]);					// convert argument 1

	if ((taskId > 0) && tcb[taskId].name)			// check for single task
	{
		my_printf("\nKill Task %d", taskId);
		tcb[taskId].state = S_EXIT;
		return 0;
	}
	else if (taskId < 0)									// check for all tasks
	{
		printf("\nKill All Tasks");
		for (taskId=1; taskId<MAX_TASKS; taskId++)
		{
			if (tcb[taskId].name)
			{
				my_printf("\nKill Task %d", taskId);
				tcb[taskId].state = S_EXIT;
			}
		}
	}
	else														// invalid argument
	{
		my_printf("\nIllegal argument or Invalid Task");
	}
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
	SEM_SIGNAL(s1Sem);
	return 0;
} // end signal

int P2_signal2(int argc, char* argv[])		// signal2
{
	SEM_SIGNAL(s2Sem);
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


// ***********************************************************************
// ***********************************************************************
// timerTask
int timerTask(int argc, char* argv[])
{
	int i;							// local task variable
	extern Semaphore* tics10sec;
	swapTask();
	while (1)
	{
		SEM_WAIT(tics10sec);
		printf("\n10sec-%s", argv[0]);
		swapTask();
	}
	return 0;						// terminate task
} // end timerTask