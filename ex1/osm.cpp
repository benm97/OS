#include "osm.h"

#include <iostream>
#include <sys/time.h>
#include <cmath>

#define DEF_ITER 1000
#define FAIL -1
#define MS_NANO 1000
#define UNROLL 10

/* Initialization function that the user must call
 * before running any other library function.
 * The function may, for example, allocate memory or
 * create/open files.
 * Pay attention: this function may be empty for some desings. It's fine.
 * Returns 0 uppon success and -1 on failure
 */
int osm_init();


/* finalizer function that the user must call
 * after running any other library function.
 * The function may, for example, free memory or
 * close/delete files.
 * Returns 0 uppon success and -1 on failure
 */
int osm_finalizer();


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations)
{
    int x;
    if (iterations == 0)
    {
        iterations = DEF_ITER;
    }
    int n_iter = int(ceil(double(iterations) / UNROLL));
    timeval time_a{}, time_b{};
    if (gettimeofday(&time_a, nullptr) == -1)
    {
        return FAIL;
    }
    for (int i = 0; i < n_iter; i++)
    {
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;
        x =1 + 1;


    }
    if (gettimeofday(&time_b, nullptr) == -1)
    {
        return FAIL;
    }
    x=MS_NANO;
    double result = (time_b.tv_usec - time_a.tv_usec) / double(iterations);
    return result * x;
}

/*
 * empty function
 */
void empty(){

}
/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations){

    if (iterations == 0)
    {
        iterations = DEF_ITER;
    }
    int n_iter = int(ceil(double(iterations) / UNROLL));
    timeval time_a{}, time_b{};
    if (gettimeofday(&time_a, nullptr) == -1)
    {
        return FAIL;
    }
    for (int i = 0; i < n_iter; i++)
    {
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();
        empty();


    }
    if (gettimeofday(&time_b, nullptr) == -1)
    {
        return FAIL;
    }
    double result = (time_b.tv_usec - time_a.tv_usec) / double(iterations);

    return result * MS_NANO;
}

/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations){
    if (iterations == 0)
    {
        iterations = DEF_ITER;
    }
    int n_iter = int(ceil(double(iterations) / UNROLL));
    timeval time_a{}, time_b{};
    if (gettimeofday(&time_a, nullptr) == -1)
    {
        return FAIL;
    }
    for (int i = 0; i < n_iter; i++)
    {
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;

    }
    if (gettimeofday(&time_b, nullptr) == -1)
    {
        return FAIL;
    }
    double result = (time_b.tv_usec - time_a.tv_usec) / double(iterations);
    return result * MS_NANO;
}

