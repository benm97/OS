//
// Created by benm on 4/7/19.
//

#ifndef EX2OS_THREAD_H
#define EX2OS_THREAD_H
#include <setjmp.h>
#include <iostream>
#include "uthreads.h"

#define READY "READY"

class Thread
{
public:
    int id;
    int quantums;
    bool isSleep;
    bool isBlock;
    char stack[4096];
    sigjmp_buf env;
    explicit Thread(int id);
    ~Thread() = default;
};


#endif //EX2OS_THREAD_H
