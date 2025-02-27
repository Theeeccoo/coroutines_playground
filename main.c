#include <stdbool.h>
#include <stdio.h>
#include "coroutine.h"

void counter(void *arg)
{
    long int n = (int)(long int)arg;
    for (int i = 0; i < n; ++i)
    {
	printf("[%zu] %d\n", coroutine_id(), i);
	coroutine_yield();
    }
}

int main(void)
{
    coroutine_init();
    printf("[%zu]\n", coroutine_id());
    coroutine_go(&counter, (void*) 5);
    coroutine_go(&counter, (void*) 10);
    while(coroutines_alive() > 1) coroutine_yield();
    coroutine_go(&counter, (void*) 4);
    coroutine_go(&counter, (void*) 2);
    coroutine_go(&counter, (void*) 5);
    while(coroutines_alive() > 1) coroutine_yield();
    coroutine_finish();
    return 0;
}
