#include "syscall.h"

int main(){
  if(Close(21)) MSG("Failed on closing file");
  else MSG("PASSED");
}