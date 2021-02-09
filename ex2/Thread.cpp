//
// Created by benm on 4/7/19.
//


#include "Thread.h"

Thread::Thread(int id)
{
    Thread::id=id;
    quantums=0;
    isSleep= false;
    isBlock=false;
}