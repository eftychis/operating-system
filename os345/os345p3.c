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

// project 3 struct
typedef struct {
	int time;
	Semaphore* sem;
} DeltaElem;


// project 3 global variables
DeltaElem* DC;
int numDCElem;
Semaphore* dcChange;
int numCars;
int numVisitors;
int numDrivers;
Semaphore* dcMutex;
// visitor semaphores
Semaphore* enterPark;
Semaphore* enterMuseum;
Semaphore* enterGiftShop;
// semaphores for car interaction
Semaphore* getPassenger;
Semaphore* seatTaken;
Semaphore* passengerSeated;
// place to temp push passenger rideOver semaphore
Semaphore* passengerROSem;
// place to push car id for driver
int carID;
// semaphores for driver interaction
Semaphore* needDriverMutex;
Semaphore* wakeupDriver;
Semaphore* gotDriver;
Semaphore* needTicket;
Semaphore* ticketReady;
Semaphore* buyTicket;
Semaphore* tickets;

// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);


// ***********************************************************************
// P3_monitorTask
int P3_monitorTask(int argc, char* argv[])
{

	return 0;
}

// ***********************************************************************
// P3_carTask
int P3_carTask(int argc, char* argv[])
{
	Semaphore* timerS;
	int ID,i;
	char buf[32];
	//extern JPARK myPark;
	Semaphore* ROs[NUM_SEATS];
	Semaphore* driverRO;

	ID = numCars++;									SWAP;

	sprintf(buf, "car[%d]", ID);					SWAP;
	timerS = createSemaphore(buf,BINARY,0);			SWAP;

	while(myPark.numExitedPark < NUM_VISITORS)
	{
		//int time = rand()%30;						SWAP;
		//P3_dcInsert(time,timerS);					SWAP;
		//if (myPark.cars[ID].location == 33)
		//{
			for (i = 0; i < NUM_SEATS; i++)
			{
				SEM_WAIT(fillSeat[ID]); 		SWAP;	// wait for available seat

				SEM_SIGNAL(getPassenger); 		SWAP;	// signal for visitor
				SEM_WAIT(seatTaken);	 		SWAP;	// wait for visitor to reply

				//... save passenger ride over semaphore ...
				ROs[i] = passengerROSem;	 		SWAP;

				SEM_SIGNAL(passengerSeated);	SWAP;	// signal visitor in seat

				
				// if last passenger, get driver
				if(i == NUM_SEATS - 1)
				{
					SEM_WAIT(needDriverMutex);	SWAP;
					// push ID
					carID = ID;	 		SWAP;
	  				// wakeup attendant
	  				SEM_SIGNAL(wakeupDriver);	SWAP;
					SEM_WAIT(gotDriver);	SWAP;
					//... save driver ride over semaphore ...
					driverRO = passengerROSem;	SWAP;

					// got driver (mutex)
					SEM_SIGNAL(needDriverMutex);	SWAP;
				}
				

				SEM_SIGNAL(seatFilled[ID]); 	SWAP;	// signal ready for next seat
			}
			
			// if car full, wait until ride over
			SEM_WAIT(rideOver[ID]);		SWAP;

			//... release passengers and driver ...
			for (i = 0; i < NUM_SEATS; i++)
			{
				SEM_SIGNAL(ROs[i]);						SWAP;
			}
			SEM_SIGNAL(driverRO);	SWAP;


		//}
		SWAP;
	}

	return 0;

} // P3_carTask

// ***********************************************************************
// P3_visitorTask
int P3_visitorTask(int argc, char* argv[])
{
	Semaphore* timerS;
	int ID,i,time;
	char buf[32];
	//extern JPARK myPark;
	Semaphore* myRideOver;

	ID = numVisitors++;									SWAP;

	sprintf(buf, "visitor[%d]", ID);					SWAP;
	timerS = createSemaphore(buf,BINARY,0);			SWAP;
	sprintf(buf, "vRO[%d]", ID);						SWAP;
	myRideOver = createSemaphore(buf,BINARY,0);			SWAP;

	// declare existance outside park
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numOutsidePark++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;

	// rand time before park entrance attempt (in line)
	time = rand()%100;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);						SWAP;
	// attempt to enter park (still in line)
	SEM_WAIT(enterPark);						SWAP;
	// enter park and ticket line
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numOutsidePark--;						SWAP;
	myPark.numInPark++;						SWAP;
	myPark.numInTicketLine++;						SWAP;
	//myPark.numInMuseumLine++; 						SWAP;	// skipping ticket line for now
	SEM_SIGNAL(parkMutex);						SWAP;

	
	// rand time before get ticket attempt (in line)
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);						SWAP;
	// attempt to get ticket (still in line)
	SEM_WAIT(tickets);			SWAP;
	SEM_WAIT(needDriverMutex);			SWAP;
	SEM_SIGNAL(needTicket);			SWAP;
	SEM_SIGNAL(wakeupDriver);			SWAP;
	SEM_WAIT(ticketReady);			SWAP;

	// rand time before buying ticket (exchange with driver)
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);	 		SWAP;
	// buy ticket from driver
	SEM_SIGNAL(buyTicket);			SWAP;
	//SEM_WAIT(needTicket);			SWAP;
	SEM_SIGNAL(needDriverMutex);			SWAP;

	// out of ticketline into museumline
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInTicketLine--;						SWAP;
	myPark.numInMuseumLine++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;	

	// rand time before enter museum attempt (in line)
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);						SWAP;
	// attempt to enter museum (still in line)
	SEM_WAIT(enterMuseum);						SWAP;
	// enter museum
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInMuseumLine--;						SWAP;
	myPark.numInMuseum++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;

	// rand time in museum
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);						SWAP;
	// exit museum into car line
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInMuseum--;						SWAP;
	myPark.numInCarLine++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;
	SEM_SIGNAL(enterMuseum);						SWAP;

	// rand time before get in car attempt (in line)
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);						SWAP;
	// attempt to get car (still in line)
	SEM_WAIT(getPassenger);						SWAP;
	passengerROSem = myRideOver;						SWAP;
	SEM_SIGNAL(seatTaken);						SWAP;

	SEM_WAIT(passengerSeated);						SWAP;
	// get in car
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInCarLine--;						SWAP;
	myPark.numInCars++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;
	myPark.numTicketsAvailable++;			SWAP;
	SEM_SIGNAL(tickets);			SWAP;


	SEM_WAIT(myRideOver);						SWAP;
	// get out of car into giftshop line
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInCars--;						SWAP;
	myPark.numInGiftLine++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;

	// rand time before enter giftshop attempt (in line)
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;

	SEM_WAIT(timerS);						SWAP;
	// attempt to enter giftshop (still in line)
	SEM_WAIT(enterGiftShop);						SWAP;
	// enter giftshop
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInGiftLine--;						SWAP;
	myPark.numInGiftShop++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;

	// rand time in giftshop
	time = rand()%30;						SWAP;
	SEM_WAIT(dcMutex);						SWAP;
	P3_dcInsert(time,timerS);					SWAP;
	SEM_SIGNAL(dcMutex);						SWAP;
	
	SEM_WAIT(timerS);						SWAP;
	// exit giftshop and park
	SEM_WAIT(parkMutex);						SWAP;
	myPark.numInGiftShop--;						SWAP;
	myPark.numInPark--;						SWAP;
	myPark.numExitedPark++;						SWAP;
	SEM_SIGNAL(parkMutex);						SWAP;
	SEM_SIGNAL(enterGiftShop);						SWAP;
	SEM_SIGNAL(enterPark);						SWAP;

	return 0;
} // P3_visitorTask

// ***********************************************************************
// P3_driverTask
int P3_driverTask(int argc, char* argv[])
{
	Semaphore* timerS;
	int ID,i,time;
	char buf[32];
	//extern JPARK myPark;
	Semaphore* myRideOver;

	ID = numDrivers++;									SWAP;

	sprintf(buf, "driver[%d]", ID);					SWAP;
	timerS = createSemaphore(buf,BINARY,0);			SWAP;
	sprintf(buf, "dRO[%d]", ID);						SWAP;
	myRideOver = createSemaphore(buf,BINARY,0);			SWAP;

	while (myPark.numExitedPark < NUM_VISITORS)
	{
		int attempt;

		SEM_WAIT(wakeupDriver);			SWAP;
		attempt = SEM_TRYLOCK(needTicket);			SWAP;
		if (attempt)
		{
			myPark.drivers[ID] = -1;			SWAP;
			//SEM_WAIT(tickets);			SWAP;
			myPark.numTicketsAvailable--;			SWAP;
			SEM_SIGNAL(ticketReady);			SWAP;
			SEM_WAIT(buyTicket);			SWAP;
			//SEM_SIGNAL(needTicket);			SWAP;
		}
		else
		{
			myPark.drivers[ID] = carID + 1;			SWAP;
			passengerROSem = myRideOver;			SWAP;
			SEM_SIGNAL(gotDriver);			SWAP;
			SEM_WAIT(myRideOver);			SWAP;
		}

		myPark.drivers[ID] = 0;			SWAP;
	}

	return 0;
} // P3_driverTask


// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	extern Semaphore* tics10thsec;

	while(1)
	{
		SEM_WAIT(tics10thsec);				SWAP;

		SEM_WAIT(dcMutex);				SWAP;
		if (numDCElem > 0)
		{
			DeltaElem* temp = &DC[numDCElem-1];				SWAP;
			temp->time--;				SWAP;
			while (temp->time <= 0)
			{
				//printf("\nSomething changed!");				SWAP;
				SEM_SIGNAL(temp->sem);				SWAP;
				numDCElem--;				SWAP;
				//SEM_SIGNAL(dcChange);				SWAP;
				temp = &DC[numDCElem-1];				SWAP;
			}
		}
		SEM_SIGNAL(dcMutex);				SWAP;
	}
	//printf("\nTo Be Implemented!");
	return 0;
} // end CL3_dcPrint

// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char ibuf[32];
	char* newArgv[2];
	int i,numVisitors;

	// set global numCars
	numCars = 0;			SWAP;
	
	dcMutex = createSemaphore("dcMutex",BINARY,1);			SWAP;
	// malloc deltaclock
	SEM_WAIT(dcMutex);	 		SWAP;
	DC = (DeltaElem*)malloc(MAX_TASKS * sizeof(DeltaElem));	 		SWAP;
	numDCElem = 0;	 		SWAP;
	SEM_SIGNAL(dcMutex);		 		SWAP;

	// start park
	sprintf(buf, "jurassicPark");			SWAP;
	newArgv[0] = buf;			SWAP;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument
				SWAP;
	// start deltaclock
	dcChange = createSemaphore("deltaSem",COUNTING,0);			SWAP;
	createTask( "deltaClock",				// task name
		P3_dc,				// task
		HIGHEST_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument
				SWAP;
	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");			SWAP;

	// set up visitor semaphores
	enterPark = createSemaphore("enterPark",COUNTING,MAX_IN_PARK);			SWAP;
	enterMuseum = createSemaphore("enterMuseum",COUNTING,MAX_IN_MUSEUM);			SWAP;
	enterGiftShop = createSemaphore("enterGiftShop",COUNTING,MAX_IN_GIFTSHOP);			SWAP;
	// set up car/visitor interactive semaphores
	getPassenger = createSemaphore("getPassenger",BINARY,0);			SWAP;
	seatTaken = createSemaphore("seatTaken",BINARY,0);			SWAP;
	passengerSeated = createSemaphore("passengerSeated",BINARY,0);			SWAP;
	// setup driver interactive semaphores
	needDriverMutex = createSemaphore("needDriverMutex",BINARY,1);			SWAP;
	wakeupDriver = createSemaphore("wakeupDriver",BINARY,0);			SWAP;
	gotDriver = createSemaphore("gotDriver",BINARY,0);			SWAP;
	needTicket = createSemaphore("needTicket",BINARY,0);			SWAP;
	ticketReady = createSemaphore("ticketReady",BINARY,0);			SWAP;
	buyTicket = createSemaphore("butTicket",BINARY,0);			SWAP;
	tickets = createSemaphore("tickets",COUNTING,MAX_TICKETS);			SWAP;


	//?? create car, driver, and visitor tasks here
	for(i = 0; i < NUM_CARS; i++)
	{
		sprintf(buf, "carTask[%d]", i);			SWAP;
		newArgv[0] = buf;			SWAP;
		sprintf(ibuf, "%d", i);			SWAP;
		newArgv[1] = ibuf;			SWAP;
		createTask( buf,				// task name
			P3_carTask,				// task
			MED_PRIORITY,				// task priority
			1,								// task count
			newArgv);					// task argument
					SWAP;
	}

	//if(!(numVisitors = strtol(argv[1], NULL, 0)))
		numVisitors = NUM_VISITORS;	 		SWAP;

	for(i = 0; i < numVisitors; i++)
	{
		sprintf(buf, "visitorTask[%d]", i);			SWAP;
		newArgv[0] = buf;			SWAP;
		sprintf(ibuf, "%d", i);			SWAP;
		newArgv[1] = ibuf;			SWAP;
		createTask( buf,				// task name
			P3_visitorTask,				// task
			MED_PRIORITY,				// task priority
			1,								// task count
			newArgv);					// task argument
					SWAP;
	}

	for(i = 0; i < NUM_DRIVERS; i++)
	{
		sprintf(buf, "driverTask[%d]", i);			SWAP;
		newArgv[0] = buf;			SWAP;
		sprintf(ibuf, "%d", i);			SWAP;
		newArgv[1] = ibuf;			SWAP;
		createTask( buf,				// task name
			P3_driverTask,				// task
			MED_PRIORITY,				// task priority
			1,								// task count
			newArgv);					// task argument
					SWAP;
	}

	// for testing cars - lost visitor screams...
	//myPark.numInCarLine = 30;

	return 0;
} // end project3


// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dcPrint(int argc, char** argv)
{
	int i;
	printf("\nDelta Clock");			SWAP;
	if (numDCElem == 0)
		return 0;

	for (i = numDCElem; i > -1; i--)
		printf("\n%4d%4d  %-20s", i, DC[i].time, DC[i].sem->name);			SWAP;

	//printf("\nTo Be Implemented!");
	return 0;
} // end CL3_dcPrint


// ***********************************************************************
// ***********************************************************************
// delta clock insert
int P3_dcInsert(int time, Semaphore* sem)
{
	int testtime,i;

	if (numDCElem == 0)
	{
		DC[0].time = time;			SWAP;
		DC[0].sem = sem;			SWAP;

		numDCElem++;			SWAP;

		return 0;
	}

	for (i = numDCElem; i > -1; i--)
	{
		if (i == 0)
		{
			DC[i].time = time;			SWAP;
			DC[i].sem = sem;			SWAP;
			break;
		}

		testtime = time - DC[i-1].time;			SWAP;
		if(testtime > 0)
		{
			DC[i].time = DC[i-1].time;			SWAP;
			DC[i].sem = DC[i-1].sem;			SWAP;
			time = testtime;			SWAP;
		}
		else
		{
			DC[i].time = time;			SWAP;
			DC[i].sem = sem;			SWAP;
			DC[i-1].time = abs(testtime);			SWAP;
			break;
		}
	}
	numDCElem++;			SWAP;

	return 0;
} // end P3_dcInsert


// ***********************************************************************
// test the delta clock
int P3_dcTest()
{
	DeltaElem* oldDC;
	int i, flg, oldNum;
	char buf[32];
	Semaphore* event[10];
	char* argv[2];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 200, 50, 170, 240, 200, 50, 200, 40, 110	};			SWAP;

	// move current values out of the way and initialize new
	oldDC = DC;			SWAP;
	oldNum = numDCElem;			SWAP;
	dcMutex = createSemaphore("dcMutex",BINARY,1);			SWAP;
	SEM_WAIT(dcMutex);			SWAP;
	DC = (DeltaElem*)malloc(MAX_TASKS * sizeof(DeltaElem));			SWAP;
	numDCElem = 0;			SWAP;
	SEM_SIGNAL(dcMutex);			SWAP;
	
	dcChange = createSemaphore("dcChange",BINARY,0);			SWAP;
	argv[0] = "testDeltaClock";			SWAP;
	createTask( "testDeltaClock",				// task name
		P3_dc,				// task
		HIGHEST_PRIORITY,				// task priority
		1,								// task count
		argv);					// task argument
				SWAP;
	for (i=0; i<10; i++)
	{
		sprintf(buf, "event[%d]", i);			SWAP;
		event[i] = createSemaphore(buf, BINARY, 0);			SWAP;
		SEM_WAIT(dcMutex);			SWAP;
		P3_dcInsert(ttime[i], event[i]);			SWAP;
		SEM_SIGNAL(dcMutex);			SWAP;
	}

	argv[0] = "P3_dc";			SWAP;
	argv[1] = "two";			SWAP;
	//P3_dc(0, argv);

	while (numDCElem > 0)
	{
		printf("\nWaiting for dcChange...");			SWAP;
		SEM_WAIT(dcChange)			SWAP;
		printf("\ndcChange signalled!");			SWAP;
		flg = 0;			SWAP;
		for (i=0; i<10; i++)
		{
			if (event[i]->state == 1)			{
					printf("\n  event[%d] signaled", i);			SWAP;
					event[i]->state = 0;			SWAP;
					flg = 1;			SWAP;
				}
		}
		if (flg) P3_dcPrint(0,argv);			SWAP;
	}
	printf("\nNo more events in Delta Clock");			SWAP;

	// move original values back and free temporaries
	free(DC);			SWAP;
	
	SEM_WAIT(dcMutex);			SWAP;
	DC = oldDC;			SWAP;
	numDCElem = oldNum;			SWAP;
	SEM_SIGNAL(dcMutex);			SWAP;
	
	return 0;
} // end P3_dcTest

/*
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printf("\nDelta Clock");
	// ?? Implement a routine to display the current delta clock contents
	//printf("\nTo Be Implemented!");
	int i;
	for (i=0; i<numDeltaClock; i++)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return 0;
} // end CL3_dc


// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void)
{
	int i;
	for (i=0; i<numDeltaClock; i++)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return;
}


// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[])
{
	createTask( "DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

	timeTaskID = createTask( "Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
	return 0;
} // end P3_tdc



// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

	for (i=0; i<10; i++)
	{
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		insertDeltaClock(ttime[i], event[i]);
	}
	printDeltaClock();

	while (numDeltaClock > 0)
	{
		SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
			if (event[i]->state ==1)			{
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
*/

