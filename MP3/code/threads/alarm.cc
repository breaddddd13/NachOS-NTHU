// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom)
{
  timer = new Timer(doRandom, this);
}

//----------------------------------------------------------------------
// aging
//   Every 100 ticks update waiting time of threads which in ready
//   queue, and when the priority changes , check if it needs to move to
//   another queue.
//----------------------------------------------------------------------
static void aging(int i){
  int level = i;
  List<Thread *> *queue = kernel->scheduler->getList(level);
  ListIterator<Thread *> *it = new ListIterator<Thread *>(queue);
  for (; !it->IsDone(); it->Next()){
    Thread * thread = it->Item();
    thread->setWaitingTick(kernel->stats->totalTicks - thread->getStartWait());
    thread->setStartWait(kernel->stats->totalTicks);
      if(thread->getWaitingTick() >= 1500) {
        int oldPriority = thread->getPriority();
        if (oldPriority < 139) thread->setPriority(oldPriority + 10);
        else thread->setPriority(149);
        DEBUG(dbgAging, "[C] Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " change its priority from " << oldPriority << " to " << thread->getPriority());
        thread->setWaitingTick(-1500);
        if(thread->getPriority() >= 50 && level == 3){
          DEBUG(dbgAging, "[B] Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is removed from queue L" << thread->getLevel());
          thread->setLevel(2);
          queue->Remove(thread);
          kernel->scheduler->getList(2)->Append(thread);
          DEBUG(dbgAging, "[A] Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L" << thread->getLevel());
        }else if(thread->getPriority() >= 100 && level == 2){
          DEBUG(dbgAging, "[B] Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is removed from queue L" << thread->getLevel());
          thread->setLevel(1);
          queue->Remove(thread);
          kernel->scheduler->getList(1)->Append(thread);
          DEBUG(dbgAging, "[A] Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L" << thread->getLevel());
        }
      }
    }
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as
//	if the interrupted thread called Yield at the point it is
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice
//      if we're currently running something (in other words, not idle).
//----------------------------------------------------------------------

void Alarm::CallBack()
{
  Interrupt *interrupt = kernel->interrupt;
  MachineStatus status = interrupt->getStatus();

  if (status != IdleMode)
  {
    for (int i = 1; i <= 3; i++) aging(i);
    if(kernel->currentThread->getLevel() != 2){
      interrupt->YieldOnReturn();
    }else{
      if (!kernel->scheduler->getList(1)->IsEmpty()){
        interrupt->YieldOnReturn();
      }
    }
  }
}


