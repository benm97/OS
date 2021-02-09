//
// Created by benm on 4/7/19.
//
#include <iostream>
#include <setjmp.h>
#include <signal.h>
#include "uthreads.h"
#include "Thread.h"
#include "Schedule.h"
#include "sleeping_threads_list.h"
using namespace std;
int quantum;
Schedule *scheduler;

SleepingThreadsList *sleeping;
int numThreads;
sigset_t my_sigset;
void real_timer_handler(int sig);
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
        "rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif



void block_signals(){
    bool flag = false;
    if(sigaddset(&my_sigset, SIGVTALRM)){
        flag = true;
    }
    if(sigaddset(&my_sigset, SIGALRM)){
        flag = true;
    }
    if(sigemptyset(&my_sigset)){
        flag = true;
    }
    if(sigprocmask(SIG_BLOCK, &my_sigset, NULL)){
        flag = true;
    }
    if(flag){
        cerr << "thread library error: Block error" << endl;
        exit(1);
    }
}

void unblock_signals(){
    bool flag = false;
    if(sigaddset(&my_sigset, SIGVTALRM)){
        flag = true;
    }
    if(sigaddset(&my_sigset, SIGALRM)){
        flag = true;
    }
    if(sigemptyset(&my_sigset)){
        flag = true;
    }
    if(sigprocmask(SIG_UNBLOCK, &my_sigset, NULL)){
        flag = true;
    }
    if(flag){
        cerr << "thread library error: Unblock error" << endl;
        exit(1);
    }
}


timeval calc_wake_up_timeval(int usecs_to_sleep) {

    timeval now{};
    timeval time_to_sleep{};
    timeval wake_up_timeval{};
    gettimeofday(&now, nullptr);
    time_to_sleep.tv_sec = usecs_to_sleep / 1000000;
    time_to_sleep.tv_usec = usecs_to_sleep % 1000000;
    timeradd(&now, &time_to_sleep, &wake_up_timeval);
    return wake_up_timeval;
}


void timer_handler(int sig)
{

    scheduler->switchThreads();

}
void runRealTimer(unsigned int usec){

    struct sigaction rSa = {nullptr};
    struct itimerval rTimer{};

    // Install timer_handler as the signal handler for SIGALRM.
    rSa.sa_handler = &real_timer_handler;
    if (sigaction(SIGALRM, &rSa, nullptr) < 0) {
        cerr << "thread library error: Sigaction error" << endl;
        exit(1);
    }

    // Configure the timer to expire after 1 sec... */
    rTimer.it_value.tv_sec = usec/1000000;		// first time interval, seconds part
    rTimer.it_value.tv_usec = usec%1000000;		// first time interval, microseconds part


    // Start a real timer.
    if (setitimer (ITIMER_REAL, &rTimer, nullptr)) {
        cerr << "thread library error: Sigitimer error" << endl;
        exit(1);
    }

}
void real_timer_handler(int sig)
{
    if(!sleeping->peek()){
        unblock_signals();
        return;
    }


    scheduler->threads[sleeping->peek()->id]->isSleep = false;
    if(!(scheduler->threads[sleeping->peek()->id]->isBlock)){
        scheduler->readyQ.push_back(scheduler->threads[sleeping->peek()->id]);
    }
    sleeping->pop();

    if(!sleeping->peek()){
        unblock_signals();
        return;
    }


    timeval now{},  remain{};
    gettimeofday(&now, nullptr);


    if (timercmp(&now, &sleeping->peek()->awaken_tv, <)){
        timersub(&sleeping->peek()->awaken_tv, &now, &remain);
        runRealTimer(remain.tv_sec*1000000+remain.tv_usec);
    } else{
        real_timer_handler(3);
    }


}


void runTimer(){

    struct sigaction sa = {nullptr};
    struct itimerval timer{};

    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        cerr << "thread library error: Sigaction error" << endl;
        exit(1);
    }

    // Configure the timer to expire after 1 sec... */
    timer.it_value.tv_sec = quantum/1000000;		// first time interval, seconds part
    timer.it_value.tv_usec = quantum%1000000;		// first time interval, microseconds part

    // configure the timer to expire every 3 sec after that.
    timer.it_interval.tv_sec = quantum/1000000;	// following time intervals, seconds part
    timer.it_interval.tv_usec = quantum%1000000;	// following time intervals, microseconds part

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
        cerr << "thread library error: Sigitimer error" << endl;
        exit(1);
    }
}
int smallestId()
{
    for (unsigned int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        if (scheduler->threads[i] == nullptr)
        {
            return i;
        }
    }
    return -1;
}

void free(){

    for (auto &thread : scheduler->threads)
    {
//        delete thread;
        thread = nullptr;
    }
    delete scheduler;
    scheduler = nullptr;
}
/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        cerr << "thread library error: Non positive quantum number" << endl;
        return -1;
    }
    quantum = quantum_usecs;
    numThreads = 0;

    try{
        scheduler = new Schedule();}
    catch (bad_alloc &bad_alloc1){
        cerr << "thread library error: Can't create a sheduler (memory pb)" << endl;
        exit(1);
    }
    for (auto &thread : scheduler->threads)
    {
        thread = nullptr;
    }

    sleeping=new SleepingThreadsList();
    uthread_spawn(nullptr);
    scheduler->running= scheduler->readyQ.front();
    scheduler->readyQ.pop_front();
    runTimer();
    scheduler->totalQuantums++;
    scheduler->running->quantums++;

    return 0;
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)())
{
    block_signals();
    if (numThreads >= MAX_THREAD_NUM)
    {
        cerr << "thread library error: Max number of threads" << endl;
        unblock_signals();
        return -1;
    }

    int id = smallestId();
    try
    {
        auto newThread = new Thread(id);
        scheduler->threads[id] = newThread;
        scheduler->readyQ.push_back(newThread);
        address_t sp, pc;
        sp = (address_t) newThread->stack + STACK_SIZE - sizeof(address_t);
        pc = (address_t) f;
        sigsetjmp(newThread->env, 1);
        (newThread->env->__jmpbuf)[JB_SP] = translate_address(sp);
        (newThread->env->__jmpbuf)[JB_PC] = translate_address(pc);
        sigemptyset(&newThread->env->__saved_mask);
    }
    catch (bad_alloc &bad_alloc1){
        cerr << "thread library error: Can't create a new thread (memory pb)" << endl;
        unblock_signals();
        exit(1);
    }
    numThreads++;
    unblock_signals();
    return id;
}

/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
    block_signals();
    if(tid==0){ //terminate the main thread and releasing memory
        free();
        unblock_signals();
        exit(0);
    }
    if(tid<0 || tid>=MAX_THREAD_NUM|| scheduler->threads[tid]== nullptr){
        cerr << "thread library error: Invalid tid number" << endl;
        unblock_signals();
        return -1;
    }

    for(unsigned int i=0;i<sleeping->sleeping_threads.size();i++){
        if(sleeping->sleeping_threads[i].id==tid){
            sleeping->sleeping_threads.erase(sleeping->sleeping_threads.begin()+i);
        }
    }

    scheduler->eraseById(tid);
    scheduler->threads[tid]= nullptr;
    numThreads--;
    unblock_signals();

    return 0;
}

/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
    block_signals();
    if(tid==0){ //Main threads
        unblock_signals();
        cerr << "thread library error: Can't block the main thread" << endl;
        return -1;
    }
    if(tid<0 || tid>=MAX_THREAD_NUM|| scheduler->threads[tid]== nullptr){
        unblock_signals();
        cerr << "thread library error: Invalid tid number (can't block)" << endl;
        return -1;
    }
    if(tid==scheduler->running->id){
        runTimer();
    }
    scheduler->blockById(tid);
    unblock_signals();
    return 0;
}

/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    block_signals();
    if(tid<0 || tid>=MAX_THREAD_NUM|| scheduler->threads[tid]== nullptr){
        cerr<< "thread library error: Invalid tid number (can't resume)" << endl;
        unblock_signals();
        return -1;
    }

    scheduler->resumeById(tid);

    unblock_signals();
    return 0;
}

/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid(){
    if(scheduler->running == nullptr){
        cerr << "thread library error: Invalid rnning" << endl;
        return -1;
    }
    return scheduler->running->id;
}

/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums(){
    return scheduler->totalQuantums;
}

/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid){
    if(tid < 0  || tid>=MAX_THREAD_NUM|| scheduler->threads[tid]== nullptr){
        cerr << "thread library error: Invalid quantums number or not existing" << endl;
        return -1;
    }
    return scheduler->threads[tid]->quantums;
}

/*
 * Description: This function blocks the RUNNING thread for usecs micro-seconds in real time (not virtual
 * time on the cpu). It is considered an error if the main thread (tid==0) calls this function. Immediately after
 * the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY threads list.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sleep(unsigned int usec){
    block_signals();


    if(scheduler->running->id==0){
        cerr<< "thread library error: Main thread goes to sleep" << endl;
        unblock_signals();
        return -1;
    }
    if(usec==0){
        unblock_signals();
        return 0;
    }
    bool wasEmpty=!sleeping->peek();
    sleeping->add(scheduler->running->id,calc_wake_up_timeval(usec));
    scheduler->running->isSleep=true;
    if(wasEmpty){
        runRealTimer(usec);
    }


    int ret_val = sigsetjmp(scheduler->running->env,1);
    if (ret_val == 1)
    {
        unblock_signals();
        return 0;
    }


    scheduler->running = scheduler->readyQ.front();
    scheduler->readyQ.pop_front();
    scheduler->running->quantums++;
    scheduler->totalQuantums++;
    unblock_signals();
    siglongjmp(scheduler->running->env, 1);


}