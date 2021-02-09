//
// Created by benm on 4/7/19.
//

#ifndef EX2OS_SCHEDULE_H
#define EX2OS_SCHEDULE_H


#include <deque>
#include "Thread.h"
using namespace std;
class Schedule
{
public:
    int totalQuantums;
    ~Schedule();
    Thread *threads[MAX_THREAD_NUM];
    Thread* running;
    deque<Thread*> readyQ;
    void eraseById(int id);
    void blockById(int id);
    void resumeById(int id);
    void switchThreads();
};
//


#endif //EX2OS_SCHEDULE_H
