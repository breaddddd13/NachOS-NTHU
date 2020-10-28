/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__ 
#define __USERPROG_KSYSCALL_H__ 

#include "kernel.h"

#include "synchconsole.h"


void SysHalt()
{
  kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
  return op1 + op2;
}

#ifdef FILESYS_STUB
int SysCreate(char *filename)
{
	// return value
	// 1: success
	// 0: failed
	return kernel->interrupt->CreateFile(filename);
}
#endif

// TODO
int SysCreate(char *filename, int size){
  (void)kernel->fileSystem->Create(filename, size);
  return 1;
}

OpenFileId SysOpen(char * name){
  return (kernel->fileSystem->Open(name) != NULL) ? 1 : 0;
}

int SysRead(char *buf, int size, OpenFileId id){
  return kernel->fileSystem->currentFile->Read(buf, size);
}

int SysWrite(char * buf, int size, OpenFileId id){
  return kernel->fileSystem->currentFile->Write(buf, size);
}

int SysClose(OpenFileId id){
  delete kernel->fileSystem->currentFile;
  return 1;
}
#endif /* ! __USERPROG_KSYSCALL_H__ */
