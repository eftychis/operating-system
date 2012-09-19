// os345p1.c - Command Line Processor
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
#include <time.h>	// included for date command

// The 'reset_context' comes from 'main' in os345.c.  Proper shut-down
// procedure is to long jump to the 'reset_context' passing in the
// power down code from 'os345.h' that indicates the desired behavior.

extern jmp_buf reset_context;
// -----


#define NUM_COMMANDS 52
typedef struct								// command struct
{
	char* command;
	char* shortcut;
	int (*func)(int, char**);
	char* description;
} Command;

// ***********************************************************************
// project 1 variables
//
#define MAX_RECALLS		10				// number of remembered commands

extern long swapCount;					// number of scheduler cycles
extern char inBuffer[];					// character input buffer
extern Semaphore* inBufferReady;		// input buffer ready semaphore
extern bool diskMounted;				// disk has been mounted
extern char dirPath[];					// directory path
Command** commands;						// shell commands
char recalls[MAX_RECALLS][INBUF_SIZE+1];
int curRecall;
int lookRecall;


// ***********************************************************************
// project 1 prototypes
Command** P1_init(void);
Command* newCommand(char*, char*, int (*func)(int, char**), char*);


// *****************************
// mySIGxHandlers
void mySIGCONTHandler(void)
{
	extern int shell_only;
	shell_only = FALSE;					// release scheduler
	return;
}

void mySIGINTHandler(void)
{
	int i;
	extern TCB tcb[MAX_TASKS];
	Semaphore* hisEvent;
	extern int numTasks;
	extern int* rq;

	sigSignal(-1, mySIGTERM);

	for (i = 1; i < MAX_TASKS; i++)
	{
		if(tcb[i].state == S_BLOCKED)
		{
			hisEvent = tcb[i].event;
			P2_unQ(hisEvent->blockedQ, hisEvent->numBlocked, i);
			hisEvent->numBlocked--;
			tcb[i].state = S_EXIT;
			P2_enQ(rq,numTasks,i);
			numTasks++;
		}
	}

	return;
}

void mySIGTERMHandler(void)
{
	extern int curTask;
	//printf("curTask = %d", curTask);
	killTask(curTask);
	return;
}

void mySIGTSTPHandler(void)
{
	sigSignal(-1, mySIGSTOP);
	return;
}

void mySIGDBUGHandler(void)
{
	int i;
	extern int shell_only;
	for (i = 1; i < MAX_TASKS; i++)		// pause everything but the shell
		sigSignal(i, mySIGSTOP);
	shell_only = TRUE;					// set global signifier for scheduler
	return;
}


// ***********************************************************************
// myShell - command line interpreter
//
// Project 1 - implement a Shell (CLI) that:
//
// 1. Prompts the user for a command line.
// 2. WAIT's until a user line has been entered.
// 3. Parses the global char array inBuffer.
// 4. Creates new argc, argv variables using malloc.
// 5. Searches a command list for valid OS commands.
// 6. If found, perform a function variable call passing argc/argv variables.
// 7. Supports background execution of non-intrinsic commands.
//
int P1_shellTask(int argc, char* argv[])
{
	int i, found, newArgc, j;					// # of arguments
	char** newArgv;							// pointers to arguments
	curRecall = 0;							// init Recall index
	lookRecall = -1;							// init Recall browsing index

	// initialize shell commands
	commands = P1_init();					// init shell commands
	
	// set handlers
	sigAction(mySIGCONTHandler, mySIGCONT);
	sigAction(mySIGINTHandler, mySIGINT);
	sigAction(mySIGTERMHandler, mySIGTERM);
	sigAction(mySIGTSTPHandler, mySIGTSTP);
	sigAction(mySIGDBUGHandler, mySIGDBUG);

	while (1)
	{
		// output prompt
		if (diskMounted) printf("\n%s>>", dirPath);
		else printf("\n%ld>>", swapCount);

		SEM_WAIT(inBufferReady);			// wait for input buffer semaphore
		if (!inBuffer[0]) continue;		// ignore blank lines
		// printf("%s", inBuffer);

		SWAP										// do context switch

		{
			// ?? >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			// ?? parse command line into argc, argv[] variables
			// ?? must use malloc for argv storage!
			static char *sp, *myArgv[MAX_ARGS];

			// init arguments
			newArgc = 1;
			myArgv[0] = sp = inBuffer;				// point to input string
			for (i=1; i<MAX_ARGS; i++)
				myArgv[i] = 0;

			// parse input string
			while ((sp = strchr(sp, ' ')))
			{
				*sp++ = 0;
				myArgv[newArgc++] = sp;
				// if argument begins with a '"',
				// search for an ending '"' with a space
				// or NULL after it to end the argument
				// (in the middle of a word doesn't count)
				if (*sp == '"')
				{
					char* temp = sp;
					while ((temp = strchr(temp+1, '"')) > 0)
					{
						if ((*(temp+1) == ' ') || (*(temp+1) == 0))
						{
							sp = temp;
							break;
						}
					}
				}
			}
			// instead of newArgv = myArgv;
			// use malloc for each string
			newArgv = (char**) malloc(sizeof(char*) * newArgc);
			for (i=0; i<newArgc; i++)
			{
				newArgv[i] = (char*) malloc(sizeof(char) * (strlen(myArgv[i])+1));
				strcpy(newArgv[i], myArgv[i]);
			}
		}	// ?? >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

		SWAP										// do context switch

		// make everything not in quotes lowercase
		for (i = 0; i < newArgc; i++)
		{
			// skip quoted strings (" at beginning and " at end)
			if ((newArgv[i][0] == '"') && (newArgv[i][strlen(newArgv[i]) - 1] == '"'))
				continue;
			for (j = 0; j < strlen(newArgv[i]); j++)
			{
				newArgv[i][j] = tolower(newArgv[i][j]);
			}
		}

		// look for command
		for (found = i = 0; i < NUM_COMMANDS; i++)
		{
			SWAP										// do context switch

			if (!strcmp(newArgv[0], commands[i]->command) ||
				 !strcmp(newArgv[0], commands[i]->shortcut))
			{
				// command found

				// if & as last arguments, create background task
				if (strcmp(newArgv[newArgc - 1], "&") == 0)
				{
					// eliminate & before passing arguments to new task
					free(newArgv[newArgc - 1]);
					newArgc--;

					// create task
					createTask(commands[i]->command,			// task name
						commands[i]->func,	// task
						MED_PRIORITY,	// task priority
						newArgc,			// task arg count
						newArgv);			// task argument pointers
				}
				else
				{
					// else run function now
					int retValue = (*commands[i]->func)(newArgc, newArgv);
					if (retValue) printf("\nCommand Error %d", retValue);
				}

				found = TRUE;
				break;
			}
		}
		if (!found)	printf("\nInvalid command!");

		// ?? free up any malloc'd argv parameters
		for (i = 0; i < newArgc; i++)
			free(newArgv[i]);
		free(newArgv);

		for (i=0; i<INBUF_SIZE; i++) inBuffer[i] = 0;
	}

	return 0;						// terminate task
} // end P1_shellTask


// ***********************************************************************
// ***********************************************************************
// P1 Project
//
int P1_project1(int argc, char* argv[])
{
	SWAP										// do context switch

	return 0;
} // end P1_project1



// ***********************************************************************
// ***********************************************************************
// P1 addRecall
//
int P1_addRecall()
{
	int i;

	if (curRecall >= MAX_RECALLS)
	{
		curRecall = MAX_RECALLS - 1;
		for(i = 1; i < MAX_RECALLS; i++)
			strcpy(recalls[i-1], recalls[i]);
	}

	strcpy(recalls[curRecall++], inBuffer);
	lookRecall = curRecall;

	return 0;
} // end P1 addRecall



// ***********************************************************************
// ***********************************************************************
// P1 getRecallUp
//
int P1_getRecallUp()
{
	lookRecall--;
	if (lookRecall < 0)
		lookRecall = curRecall - 1;
	if (lookRecall < 0)
		return 0;
	strcpy(inBuffer, recalls[lookRecall]);
	
	return 0;
} // end P1 addRecallUp



// ***********************************************************************
// ***********************************************************************
// P1 getRecallDown
//
int P1_getRecallDown()
{
	lookRecall++;
	//if (lookRecall >= curRecall)
	//	lookRecall = 0;
	if (lookRecall >= curRecall)
	{
		lookRecall = curRecall;
		strcpy(inBuffer, "");
		return 0;
	}
	strcpy(inBuffer, recalls[lookRecall]);
	
	return 0;
} // end P1 addRecallDown


// ***********************************************************************
// ***********************************************************************
// quit command
//
int P1_quit(int argc, char* argv[])
{
	int i;

	// free P1 commands
	for (i = 0; i < NUM_COMMANDS; i++)
	{
		free(commands[i]->command);
		free(commands[i]->shortcut);
		free(commands[i]->description);
	}
	free(commands);

	// powerdown OS345
	longjmp(reset_context, POWER_DOWN_QUIT);
	return 0;
} // end P1_quit



// **************************************************************************
// **************************************************************************
// lc3 command
//
int P1_lc3(int argc, char* argv[])
{
	strcpy (argv[0], "0");
	return lc3Task(argc, argv);
} // end P1_lc3



// ***********************************************************************
// ***********************************************************************
// add command
//
int P1_add(int argc, char* argv[])
{
	int i, answer, temp;

	SWAP										// do context switch

	answer = 0;
	// convert and add parameters to answer
	for (i = 1; i < argc; i++)
	{
		SWAP										// do context switch
		temp = strtol(argv[i], NULL, 0);
		if (temp == 0 && !(strcmp(argv[i], "0") == 0))
		{
			SWAP										// do context switch
			printf("\n%4s is not a valid number", argv[i]);
			return 0;
		}
		answer += temp;
	}

	SWAP										// do context switch

	printf("\n%4The answer is %ld", answer);

	return 0;
} // end P1_add



// ***********************************************************************
// ***********************************************************************
// args command
//
int P1_args(int argc, char* argv[])
{
	int i;
	SWAP										// do context switch

	// list parameters
	for (i = 0; i < argc; i++)
	{
		SWAP										// do context switch
		printf("\n%4d: %s", i, argv[i]);
	}

	SWAP										// do context switch
	return 0;
} // end P1_args



// ***********************************************************************
// ***********************************************************************
// date command
//
int P1_date(int argc, char* argv[])
{
	time_t rawtime;
	char t[26];
	SWAP										// do context switch

	time(&rawtime);
	strcpy(t,ctime(&rawtime));
	SWAP										// do context switch

	t[strlen(t) - 1] = 0;				// eliminate the ending \n because i don't like it
	printf("\nThe current date/time is: %s", t);

	SWAP										// do context switch
	return 0;
} // end P1_date



// ***********************************************************************
// ***********************************************************************
// help command
//
int P1_help(int argc, char* argv[])
{
	int i, j, found;
	SWAP										// do context switch

	if (argc == 1)
	{
		// list all commands
		for (i = 0; i < NUM_COMMANDS; i++)
		{
			SWAP										// do context switch
			if (strstr(commands[i]->description, ":")) printf("\n");
			printf("\n%4s: %s", commands[i]->shortcut, commands[i]->description);
		}
	}
	else if (strstr("p1p2p3p4p6", argv[1]) > 0)		// p5 is not included
	{
		// list all commands under that project
		for (i = 0; i < NUM_COMMANDS; i++)				// find the index of that project command
		{
			SWAP										// do context switch
			if (strcmp(commands[i]->shortcut, argv[1]) == 0)
			{
				j = i;
				break;
			}
		}
		// list that command and all commands under it until another
		// project command is encounter or end of command list is reached
		printf("\n%4s: %s", commands[j]->shortcut, commands[j]->description);
		for (i = j + 1; i < NUM_COMMANDS; i++)
		{
			SWAP										// do context switch
			if (strstr(commands[i]->description, ":"))
				break;
			printf("\n%4s: %s", commands[i]->shortcut, commands[i]->description);
		}
	}
	else
	{
		found = FALSE;
		// find the command and print it out
		for (i = 0; i < NUM_COMMANDS; i++)
		{
			SWAP										// do context switch
			if ((strcmp(commands[i]->shortcut, argv[1]) == 0) || (strcmp(commands[i]->command, argv[1])) == 0)
			{
				printf("\n%4s: %s", commands[i]->shortcut, commands[i]->description);
				found = TRUE;
				break;
			}
		}
		if (!found)
			printf("\n%4s: Command not found", argv[1]);
	}

	SWAP										// do context switch

	return 0;
} // end P1_help



// ***********************************************************************
// ***********************************************************************
// initialize shell commands
//
Command* newCommand(char* command, char* shortcut, int (*func)(int, char**), char* description)
{
	Command* cmd = (Command*)malloc(sizeof(Command));

	// get long command
	cmd->command = (char*)malloc(strlen(command) + 1);
	strcpy(cmd->command, command);

	// get shortcut command
	cmd->shortcut = (char*)malloc(strlen(shortcut) + 1);
	strcpy(cmd->shortcut, shortcut);

	// get function pointer
	cmd->func = func;

	// get description
	cmd->description = (char*)malloc(strlen(description) + 1);
	strcpy(cmd->description, description);

	return cmd;
} // end newCommand


Command** P1_init()
{
	int i  = 0;
	Command** commands = (Command**)malloc(sizeof(Command*) * NUM_COMMANDS);

	// system
	commands[i++] = newCommand("quit", "q", P1_quit, "Quit");
	commands[i++] = newCommand("kill", "kt", P2_killTask, "Kill task");
	commands[i++] = newCommand("reset", "rs", P2_reset, "Reset system");

	// P1: Shell
	commands[i++] = newCommand("project1", "p1", P1_project1, "P1: Shell");
	commands[i++] = newCommand("help", "he", P1_help, "OS345 Help");
	commands[i++] = newCommand("lc3", "lc3", P1_lc3, "Execute LC3 program");
	// added by me
	commands[i++] = newCommand("add", "+", P1_add, "Add all numbers in command line");
	commands[i++] = newCommand("arguments", "args", P1_args, "List all parameters on commandline");
	commands[i++] = newCommand("date", "da", P1_date, "Output current system date and time");

	// P2: Tasking
	commands[i++] = newCommand("project2", "p2", P2_project2, "P2: Tasking");
	commands[i++] = newCommand("semaphores", "sem", P2_listSems, "List semaphores");
	commands[i++] = newCommand("tasks", "lt", P2_listTasks, "List tasks");
	commands[i++] = newCommand("signal1", "s1", P2_signal1, "Signal sem1 semaphore");
	commands[i++] = newCommand("signal2", "s2", P2_signal2, "Signal sem2 semaphore");

	// P3: Jurassic Park
	commands[i++] = newCommand("project3", "p3", P3_project3, "P3: Jurassic Park");
	commands[i++] = newCommand("deltaclockprint", "dcp", P3_dcPrint, "List deltaclock entries");
	commands[i++] = newCommand("deltaclocktest", "dct", P3_dcTest, "Test deltaclock");

	// P4: Virtual Memory
	commands[i++] = newCommand("project4", "p4", P4_project4, "P4: Virtual Memory");
	commands[i++] = newCommand("frametable", "dft", P4_dumpFrameTable, "Dump bit frame table");
	commands[i++] = newCommand("initmemory", "im", P4_initMemory, "Initialize virtual memory");
	commands[i++] = newCommand("touch", "vma", P4_vmaccess, "Access LC-3 memory location");
	commands[i++] = newCommand("stats", "vms", P4_virtualMemStats, "Output virtual memory stats");
	commands[i++] = newCommand("crawler", "cra", P4_crawler, "Execute crawler.hex");
	commands[i++] = newCommand("memtest", "mem", P4_memtest, "Execute memtest.hex");

	commands[i++] = newCommand("frame", "dfm", P4_dumpFrame, "Dump LC-3 memory frame");
	commands[i++] = newCommand("memory", "dm", P4_dumpLC3Mem, "Dump LC-3 memory");
	commands[i++] = newCommand("page", "dp", P4_dumpPageMemory, "Dump swap page");
	commands[i++] = newCommand("virtual", "dvm", P4_dumpVirtualMem, "Dump virtual memory page");
	commands[i++] = newCommand("root", "rpt", P4_rootPageTable, "Display root page table");
	commands[i++] = newCommand("user", "upt", P4_userPageTable, "Display user page table");

	// P5: Scheduling
//	commands[i++] = newCommand("project5", "p5", P5_project5, "P5: Scheduling");
//	commands[i++] = newCommand("atm", "a", P5_atm, "Access ATM");
//	commands[i++] = newCommand("messages", "lm", P5_listMessages, "List messages");
//	commands[i++] = newCommand("stress1", "t1", P5_stress1, "ATM stress test1");
//	commands[i++] = newCommand("stress2", "t2", P5_stress2, "ATM stress test2");

	// P6: FAT
	commands[i++] = newCommand("project6", "p6", P6_project6, "P6: FAT");
	commands[i++] = newCommand("change", "cd", P6_cd, "Change directory");
	commands[i++] = newCommand("copy", "cf", P6_copy, "Copy file");
	commands[i++] = newCommand("define", "df", P6_define, "Define file");
	commands[i++] = newCommand("delete", "dl", P6_del, "Delete file");
	commands[i++] = newCommand("directory", "dir", P6_dir, "List current directory");
	commands[i++] = newCommand("mount", "md", P6_mount, "Mount disk");
	commands[i++] = newCommand("mkdir", "mk", P6_mkdir, "Create directory");
	commands[i++] = newCommand("run", "run", P6_run, "Execute LC-3 program");
	commands[i++] = newCommand("space", "sp", P6_space, "Space on disk");
	commands[i++] = newCommand("type", "ty", P6_type, "Type file");
	commands[i++] = newCommand("unmount", "um", P6_unmount, "Unmount disk");

	commands[i++] = newCommand("fat", "ft", P6_dfat, "Display fat table");
	commands[i++] = newCommand("fileslots", "fs", P6_fileSlots, "Display current open slots");
	commands[i++] = newCommand("sector", "ds", P6_dumpSector, "Display disk sector");
	commands[i++] = newCommand("chkdsk", "ck", P6_chkdsk, "Check disk");
	commands[i++] = newCommand("final", "ft", P6_finalTest, "Execute file test");

	commands[i++] = newCommand("open", "op", P6_open, "Open file test");
	commands[i++] = newCommand("read", "rd", P6_read, "Read file test");
	commands[i++] = newCommand("write", "wr", P6_write, "Write file test");
	commands[i++] = newCommand("seek", "sk", P6_seek, "Seek file test");
	commands[i++] = newCommand("close", "cl", P6_close, "Close file test");

	assert(i == NUM_COMMANDS);

	return commands;

} // end P1_init
