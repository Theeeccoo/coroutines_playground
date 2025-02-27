#ifndef COROUTINE_H_
#define COROUTINE_H_

size_t coroutine_id(void);
size_t coroutines_alive(void);

void coroutine_init(void);
void coroutine_go(void (*f)(void*), void* arg);
void coroutine_yield(void);
void coroutine_finish(void);

#endif // COROUTINE_H_
