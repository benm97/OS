//
// Created by benm on 4/7/19.
//

#include "Schedule.h"
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <thread>

void Schedule::switchThreads()
{
    if(readyQ.empty()){ //Only main thread running

        running = threads[0];
        running->quantums++;
        totalQuantums++;
    }
    else if(running == nullptr){
        running = readyQ.front();
        readyQ.pop_front();
        running->quantums++;
        totalQuantums++;
        siglongjmp(running->env,1);

    }

    else
    {
        int ret_val = sigsetjmp(running->env, 1);
        if (ret_val == 1)
        {
            return;
        }

        if (!(running->isBlock) && !(running->isSleep))
        {
            readyQ.push_back(running);
        }
        running = readyQ.front();
        readyQ.pop_front();
        running->quantums++;
        totalQuantums++;
        siglongjmp(running->env, 1);
    }


}





Schedule::~Schedule()
{
    delete running;
    while(!readyQ.empty()){
        delete readyQ.front();
        readyQ.pop_front();
    }

}
void Schedule::eraseById(int id)
{
    if(id==running->id){

        delete running;
        running = nullptr;
        switchThreads();
        return;
    }

    for(unsigned int i=0;i<readyQ.size();i++){
        if(readyQ[i]->id==id){
            delete readyQ[i];
            readyQ.erase(readyQ.begin()+i);
            return;
        }
    }
    for(auto & thread : threads){
        if(thread != nullptr && thread->id==id){
            delete thread;
            thread = nullptr;
            return;
        }
    }

}

void Schedule::blockById(int id)
{

    if (id == running->id)
    {
        running->isBlock = true;
        switchThreads();
        return;
    }
    for (unsigned int i = 0; i < readyQ.size(); i++)
    { //Block another thread
        if (readyQ[i]->id == id)
        {
            readyQ[i]->isBlock = true;
            readyQ.erase(readyQ.begin() + i);
            return;

        }
    }
    for(auto &thread:threads){
        if(thread != nullptr && thread->id==id){
            thread->isBlock= true;
            return;
        }
    }

}

void Schedule::resumeById(int id)
{

    for (auto thr : threads)
    {
        if (thr != nullptr && thr->id == id && thr->isBlock)
        {
            thr->isBlock = false;
            if (!(thr->isSleep))
            {
                readyQ.push_back(thr);
                return;
            }
        }
    }
}


