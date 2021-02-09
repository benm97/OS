#include <cmath>
#include <bitset>
#include "VirtualMemory.h"
#include "PhysicalMemory.h"


uint64_t extractBits(uint64_t address, int toExtract, int from)
{

    return (((1 << toExtract) - 1) & (address >> (from - 1)));
}

uint64_t concatBits(uint64_t a, uint64_t b)
{
    return (a << OFFSET_WIDTH) | b;
}

uint64_t getOffset(uint64_t address, int currDepth)
{
    int first = VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH;
    if(first==0){
        first=OFFSET_WIDTH;
    }
    if (currDepth == 0)
    {
        return extractBits(address, first, VIRTUAL_ADDRESS_WIDTH +1- first);
    }
    return extractBits(address, OFFSET_WIDTH,
                        VIRTUAL_ADDRESS_WIDTH+1 - (first + (currDepth * OFFSET_WIDTH)));
}

uint64_t getCyclic(uint64_t in, uint64_t page)
{
    uint64_t distance;
    if (in > page)
    {
        distance = in - page;
    }
    else
    {
        distance = page - in;
    }
    if (distance > NUM_PAGES - distance)
    {
        return NUM_PAGES - distance;
    }
    return distance;
}

void findEmpty(word_t prevAddr, word_t currFrameNumber, int currDepth, word_t *maxFrameIndex,
                 word_t *maxDistance, word_t *frameWithMax, word_t in, word_t path,
                 word_t *pathToMax, word_t *prevOfMax, word_t toProtect, word_t *empty,
                 word_t *emptyPrev)
{
    if (currDepth == TABLES_DEPTH)
    {
        auto cyclicalDistance = (word_t) getCyclic((uint64_t) in, (uint64_t) path); //TODO CHELOU
        if (cyclicalDistance > *maxDistance && currFrameNumber != toProtect)
        {
            *maxDistance = cyclicalDistance;
            *frameWithMax = currFrameNumber;
            *pathToMax = path;
            *prevOfMax = prevAddr;
        }
        return;
    }
    word_t value;
    word_t page[PAGE_SIZE];
    bool is_empty = true;

    for (int i = 0; i < PAGE_SIZE; ++i)
    {
        PMread((uint64_t) currFrameNumber * PAGE_SIZE + i, &value);
        page[i] = value;
        if (value != 0)
        {
            is_empty = false;


        }
        if (value > *maxFrameIndex && value+1 != toProtect)
        {
            *maxFrameIndex = value;
        }
    }
    if (is_empty && currDepth < TABLES_DEPTH  && currFrameNumber != toProtect) //TODO index?
    {

        *empty=currFrameNumber;
        *emptyPrev=prevAddr;
         return ;
    }
    for (int i = 0; i < PAGE_SIZE; ++i)
    {
         if(page[i]==0){
             continue;
         }
         findEmpty((word_t) (currFrameNumber * PAGE_SIZE + i), page[i], currDepth + 1,
                         maxFrameIndex, maxDistance, frameWithMax, in,
                         (word_t) concatBits((uint64_t) path, (uint64_t) i), pathToMax, prevOfMax,
                         toProtect,empty,emptyPrev);
    }


}

word_t getAvailable(word_t in, word_t toProtect)
{
    word_t maxFrameIndex = 0;
    word_t maxDistance = 0;
    word_t maxDistanceFrame = 0;
    word_t pathToMax = 0;
    word_t prevOfMax = 0;
    word_t empty = -1;
    word_t prevEmpty = 0;
    findEmpty(0, 0, 0, &maxFrameIndex, &maxDistance, &maxDistanceFrame, in, 0, &pathToMax,
                                 &prevOfMax, toProtect,&empty,&prevEmpty);
    if (empty > -1)
    {
        PMwrite((uint64_t) prevEmpty, 0);
        return empty;
    }
    if (maxFrameIndex + 1 < NUM_FRAMES)
    {
        return maxFrameIndex + 1;
    }
    PMwrite((uint64_t) prevOfMax, 0);
    PMevict((uint64_t) maxDistanceFrame, (uint64_t) pathToMax);
    return maxDistanceFrame;
}


word_t getFrame(uint64_t virtualAddress)
{
    int depth = 0;
    word_t pageNumber = (word_t) virtualAddress >> OFFSET_WIDTH;
    word_t addr = 0;
    word_t addrOld=0;
    word_t toProtect=0;
    while (depth < TABLES_DEPTH)
    {
        addrOld=addr;
        PMread(addr * PAGE_SIZE + getOffset(virtualAddress, depth), &addr);
        toProtect=addrOld;
        if (addr == 0)
        {

            word_t frameNumber = getAvailable(pageNumber,toProtect);
            //toProtect=frameNumber;
            if (depth == TABLES_DEPTH - 1)
            {
                PMrestore((uint64_t) frameNumber, (uint64_t) pageNumber);
            }
            else
            {
                for (int i = 0; i < PAGE_SIZE; ++i)
                {
                    PMwrite((uint64_t) frameNumber*PAGE_SIZE + i, 0);
                }
            }

            PMwrite(addrOld * PAGE_SIZE + getOffset(virtualAddress, depth), frameNumber);
            addr = frameNumber;
        }

        depth++;
    }

    return addr;
}


void clearTable(uint64_t frameIndex)
{
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

void VMinitialize()
{
    //std::cout<<getOffset(25, 4);
    clearTable(0);

}


int VMread(uint64_t virtualAddress, word_t *value)
{
    word_t frameNum = getFrame(virtualAddress);
    PMread(frameNum * PAGE_SIZE + getOffset(virtualAddress, TABLES_DEPTH), value);
    return 1;
}


int VMwrite(uint64_t virtualAddress, word_t value)
{
    word_t frameNum = getFrame(virtualAddress);
    PMwrite(frameNum * PAGE_SIZE + getOffset(virtualAddress, TABLES_DEPTH ), value);
    return 1;
}
