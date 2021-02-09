#include "MapReduceFramework.h"
#include "MapReduceClient.h"
#include "Barrier.h"
#include <iostream>
#include <algorithm>
#include <pthread.h>
#include <semaphore.h>
#include <cstdio>
#include <atomic>
#include <zconf.h>

using namespace std;

struct ThreadContext;
struct JobContext {
    bool joined;
    ThreadContext* all;
    int numThreads;
    pthread_t *threads;
    pthread_mutex_t *lockOut;
    pthread_mutex_t *lockShuffle;
    pthread_mutex_t *lockProcessed;
    std::atomic<int> *atomic_counter;
    std::atomic<int> *processed; //atomic
    std::atomic<int> *numKeys; //atomic
    std::atomic<int> *stage; //atomic
    const MapReduceClient *client;
    const InputVec inputVec;
    OutputVec *outputVec; //toprotect
    vector<IntermediateVec> *intermediateVecs;
    vector<IntermediateVec> *shuffled; //to protect
    Barrier *barrier;
    sem_t sem;


};
struct ThreadContext {
    JobContext *shared{};
    int threadIndex = 0;

};


void shuffle(void *context) {
    auto *tc = (ThreadContext *) context;
    IntermediateVec oneKeyVec;
    int emptyCount = 0;
    for (const auto &vec:*tc->shared->intermediateVecs) {
        if (vec.empty()) {
            emptyCount++;
        }
    }
    while (emptyCount < (int)tc->shared->intermediateVecs->size()) {
        oneKeyVec.clear();
        IntermediatePair max(NULL, NULL);
        for (auto vec:*tc->shared->intermediateVecs) {

            if (!vec.empty() && (max.first == NULL || *vec.front().first < *max.first)) {
                max = vec.front();
            }
        }

        for (auto &intermediateVec : *tc->shared->intermediateVecs) {
            if(intermediateVec.empty()){
                continue;
            }
            reverse(intermediateVec.begin(),intermediateVec.end()); //added
            while (!(intermediateVec.empty() || *max.first < *intermediateVec.back().first ||
                     *intermediateVec.back().first < *max.first)) {
                oneKeyVec.push_back(intermediateVec.back()); //modified
                intermediateVec.pop_back();//modified
                if (intermediateVec.empty()) {
                    emptyCount++;
                }
            }
        }
        pthread_mutex_lock(tc->shared->lockShuffle);

        tc->shared->shuffled->push_back(oneKeyVec);
        pthread_mutex_unlock(tc->shared->lockShuffle);
        sem_post(&(tc->shared->sem));
    }
    for (int i = 0; i < tc->shared->numThreads; ++i) {
        sem_post(&(tc->shared->sem));
    }
}


void *run(void *context) {

    auto *tc = (ThreadContext *) context;
    pthread_mutex_lock(tc->shared->lockProcessed); //TODO
    tc->shared->stage->store(MAP_STAGE);
    pthread_mutex_unlock(tc->shared->lockProcessed);
    tc->shared->numKeys->store((int)tc->shared->inputVec.size());


    while (true) { //TODO egal

        int old_value = (*(tc->shared->atomic_counter))++;

        if (old_value >= (int)tc->shared->inputVec.size()) {
            break;
        }

        tc->shared->client->map((tc->shared->inputVec[old_value].first), tc->shared->inputVec[old_value].second,
                                context);
        pthread_mutex_lock(tc->shared->lockProcessed);
        (*(tc->shared->processed))++;
        pthread_mutex_unlock(tc->shared->lockProcessed);


    }

    if (!tc->shared->intermediateVecs->at((unsigned)tc->threadIndex).empty()) {

        std::sort((tc->shared->intermediateVecs->at((unsigned long)tc->threadIndex)).begin(), (tc->shared->intermediateVecs->at((unsigned long)tc->threadIndex)).end(),
                  [](const IntermediatePair &one, const IntermediatePair &two) { return *one.first < *two.first; });

    }

    tc->shared->barrier->barrier();



    if (tc->threadIndex == 0) {
        pthread_mutex_lock(tc->shared->lockProcessed);
        tc->shared->stage->store(REDUCE_STAGE);
        tc->shared->processed->store(0);
        pthread_mutex_unlock(tc->shared->lockProcessed);
        int toShuffle = 0;
        for (const auto &vec:*tc->shared->intermediateVecs) {
            toShuffle += vec.size();
        }
        tc->shared->numKeys->store(toShuffle);
        shuffle(context);

    }
    while (true) {
        sem_wait(&(tc->shared->sem));
        pthread_mutex_lock(tc->shared->lockShuffle);
        if (tc->shared->shuffled->empty()) {
            pthread_mutex_unlock(tc->shared->lockShuffle);
            break;
        }

        IntermediateVec tmp = tc->shared->shuffled->back();
        tc->shared->shuffled->pop_back();
        pthread_mutex_unlock(tc->shared->lockShuffle);
        tc->shared->client->reduce(&tmp, context);
        pthread_mutex_lock(tc->shared->lockProcessed);
        (*(tc->shared->processed)) += tmp.size();
        pthread_mutex_unlock(tc->shared->lockProcessed);

    }
    return nullptr;

}


JobHandle startMapReduceJob(const MapReduceClient &client,
                            const InputVec &inputVec, OutputVec &outputVec,
                            int multiThreadLevel) {

    auto *threads = new pthread_t[multiThreadLevel];
    auto atomic_counter = new std::atomic<int>(0);
    auto processed = new std::atomic<int>(0);
    auto numKeys = new std::atomic<int>(0);
    auto stage = new std::atomic<int>(0);
    auto *barrier = new Barrier(multiThreadLevel);
    auto newVec = new vector<IntermediateVec>;
    auto newShuffled = new vector<IntermediateVec>;
    auto lockOut = new pthread_mutex_t;
    auto lockShuffle = new pthread_mutex_t;
    auto lockProcessed=new pthread_mutex_t;
    pthread_mutex_init(lockOut, nullptr);
    pthread_mutex_init(lockShuffle, nullptr);
    pthread_mutex_init(lockProcessed, nullptr);
    auto *threadContexts = new ThreadContext[multiThreadLevel];
    auto *sharedContext = new JobContext{false,threadContexts,multiThreadLevel, threads, lockOut, lockShuffle,lockProcessed, atomic_counter, processed,
                                         numKeys, stage, &client, inputVec, &outputVec, newVec, newShuffled, barrier};
    sem_init(&(sharedContext->sem), 0, 0);
    sharedContext->stage->store(UNDEFINED_STAGE);
    for (int i = 0; i < multiThreadLevel; ++i) {
        IntermediateVec a;

        sharedContext->intermediateVecs->push_back(a);
        threadContexts[i].shared = sharedContext;
        threadContexts[i].threadIndex = i;
    }
    for (int i = 0; i < multiThreadLevel; ++i) {


        pthread_create(threads + i, nullptr, run, threadContexts + i);
    }


    JobHandle myJob = reinterpret_cast <JobHandle *>(sharedContext);


    return myJob;
}

void emit2(K2 *key, V2 *value, void *context) {
    auto *tc = (ThreadContext *) context;

    IntermediatePair intermediatePair(key, value);
    (tc->shared->intermediateVecs->at((unsigned long)tc->threadIndex)).push_back(intermediatePair);
}




void emit3(K3 *key, V3 *value, void *context) {
    auto *tc = (ThreadContext *) context;
    OutputPair outputPair(key, value);
    pthread_mutex_lock(tc->shared->lockOut);
    tc->shared->outputVec->push_back(outputPair);
    pthread_mutex_unlock(tc->shared->lockOut);
}

void getJobState(JobHandle job, JobState *state) {
    auto *context = reinterpret_cast <JobContext *>(job);
    pthread_mutex_lock(context->lockProcessed);//TODO
    state->stage = (stage_t) context->stage->load(std::memory_order_relaxed);
    if (state->stage == UNDEFINED_STAGE) {
        state->percentage = 0;
    } else {
        state->percentage = ((float) context->processed->load()) / ((float) context->numKeys->load()) * 100;
    }
    pthread_mutex_unlock(context->lockProcessed);
}
void waitForJob(JobHandle job) {
//    JobState state;
//    getJobState(job,&state);
//    if(state.stage==REDUCE_STAGE && state.percentage>=100){
//        return;
//    }
    auto context = reinterpret_cast <JobContext *>(job);
    if(context->joined){
        return;
    }
    context->joined=true;
    for (int i = 0; i < context->numThreads; ++i) {

        pthread_join(((context->threads)[i]), nullptr);

    }

}
void closeJobHandle(JobHandle job) {

    auto context = reinterpret_cast <JobContext *>(job);

    waitForJob(job);

    pthread_mutex_destroy(context->lockShuffle);
    sem_destroy(&(context->sem));
    pthread_mutex_destroy(context->lockOut);
    pthread_mutex_destroy(context->lockProcessed);
    delete  context->barrier;
    delete  context->shuffled;
    delete  context->intermediateVecs;
    delete  context->atomic_counter;
    delete  context->processed;
    delete  context->stage;
    delete  context->numKeys;

    delete  context->lockShuffle;
    delete  context->lockOut;
    delete  context->lockProcessed;
    delete[] context->threads;
    delete[] context->all;
    delete (JobContext *) job;

}