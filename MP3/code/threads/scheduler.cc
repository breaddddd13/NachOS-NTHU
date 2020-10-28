// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// PredictTimeCompare
//	Compare to threads based on which burst time is shorter.
//----------------------------------------------------------------------

static int
PredictTimeCompare(Thread *x, Thread *y){
  if (x->getPredictTime() < y->getPredictTime()){
    return -1;
  }else if (x->getPredictTime() > y->getPredictTime()){
    return 1;
  }else{
    return 0;
  }
}

//----------------------------------------------------------------------
// PriorityCompare
//	Compare to threads based on which priority is higher.
//----------------------------------------------------------------------

static int
PriorityCompare(Thread *x, Thread *y)
{
  if (x->getPriority() < y->getPriority()){
    return 1; 
  }else if (x->getPriority() > y->getPriority()){
    return -1;
  }else{
    return 0;
  }
}

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------


Scheduler::Scheduler()
{
  L1queue = new SortedList<Thread *>(PredictTimeCompare);
  L2queue = new SortedList<Thread *>(PriorityCompare);
  L3queue = new List<Thread *>;
  toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{
  delete L1queue;
  delete L2queue;
  delete L3queue;
}
//----------------------------------------------------------------------
// Scheduler::PutIntoQ
//	Set the thread'l level first and, 
// 	Put the thread into it's level queue
//----------------------------------------------------------------------
void Scheduler::putIntoQ(int level, Thread * thread){
  List<Thread*> *queue = getList(level);
  thread->setLevel(level);
  queue->Append(thread);
}
//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread)
{
  ASSERT(kernel->interrupt->getLevel() == IntOff);
  DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
  //cout << "Putting thread on ready list: " << thread->getName() << endl ;
  thread->setStatus(READY);
  thread->setStartWait(kernel->stats->totalTicks); // start to wait
  if (thread->getPriority() < 50){
    putIntoQ(3, thread);
  }else if(thread->getPriority() < 100){
    putIntoQ(2, thread);
  }else{
    putIntoQ(1, thread);
  }
  DEBUG(dbgAging, "[A] Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L" << thread->getLevel());
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun()
{
  ASSERT(kernel->interrupt->getLevel() == IntOff);
  Thread *nextThread = NULL;
  if (L1queue->IsEmpty() == false){
    nextThread = L1queue->RemoveFront();
    nextThread->setWaitingTick(kernel->stats->totalTicks - nextThread->getStartWait());
    DEBUG(dbgAging, "[B] Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() << " is removed from queue L" << nextThread->getLevel());
    return nextThread;
  }else if (L2queue->IsEmpty() == false){
    nextThread = L2queue->RemoveFront();
    nextThread->setWaitingTick(kernel->stats->totalTicks - nextThread->getStartWait());
    DEBUG(dbgAging, "[B] Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() << " is removed from queue L" << nextThread->getLevel());
    return nextThread;
  }else if (L3queue->IsEmpty() == false){
    nextThread = L3queue->RemoveFront();
    nextThread->setWaitingTick(kernel->stats->totalTicks - nextThread->getStartWait());
    DEBUG(dbgAging, "[B] Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() << " is removed from queue L" << nextThread->getLevel());
    return nextThread;
  }else{
    return NULL;
  }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing)
{
  Thread *oldThread = kernel->currentThread;

  ASSERT(kernel->interrupt->getLevel() == IntOff);

  if (finishing)
  { // mark that we need to delete current thread
    ASSERT(toBeDestroyed == NULL);
    toBeDestroyed = oldThread;
  }

  if (oldThread->space != NULL)
  {                             // if this thread is a user program,
    oldThread->SaveUserState(); // save the user's CPU registers
    oldThread->space->SaveState();
  }

  oldThread->CheckOverflow(); // check if the old thread
                              // had an undetected stack overflow
                              

  DEBUG(dbgAging, "[E] Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() << " is now selected for excution, thread " << oldThread->getID() << " is replaced, and it has excuted " << oldThread->getExecStore() << " ticks");
  nextThread->setStartExec(kernel->stats->totalTicks); // first time to run
  kernel->currentThread = nextThread; // switch to the next thread
  nextThread->setStatus(RUNNING);     // nextThread is now running

  DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
  // This is a machine-dependent assembly language routine defined
  // in switch.s.  You may have to think
  // a bit to figure out what happens after this, both from the point
  // of view of the thread and from the perspective of the "outside world".

  SWITCH(oldThread, nextThread);

  // we're back, running oldThread
  oldThread->setStartExec(kernel->stats->totalTicks);

  // interrupts are off when we return from switch!
  ASSERT(kernel->interrupt->getLevel() == IntOff);

  DEBUG(dbgThread, "Now in thread: " << oldThread->getName());
      CheckToBeDestroyed(); // check if thread we were running
                            // before this one has finished
                            // and needs to be cleaned up

  if (oldThread->space != NULL)
  {                                // if there is an address space
    oldThread->RestoreUserState(); // to restore, do it.
    oldThread->space->RestoreState();
  }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed()
{
  if (toBeDestroyed != NULL)
  {
    delete toBeDestroyed;
    toBeDestroyed = NULL;
  }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print()
{
  cout << "Ready list contents:\n";
  L1queue->Apply(ThreadPrint);
  L2queue->Apply(ThreadPrint);
  L3queue->Apply(ThreadPrint);
}


List<Thread *>* Scheduler::getList(int level){
  if(level == 1){
    return L1queue;
  }else if(level == 2){
    return L2queue;
  }else {
    return L3queue;
  }
}
