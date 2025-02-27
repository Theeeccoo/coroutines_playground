#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef enum { false, true } bool;

#define STACK_CAPACITY (4*1024)

// Initial capacity of a dynamic array
#ifndef DA_INIT_CAP
#define DA_INIT_CAP 256
#endif

// Append an item to a dynamic array
#define da_append(da, item)                                                         \
    do {                                                                            \
       if ((da)->count >= (da)->capacity) {                                         \
	   (da)->capacity = (da)->capacity == 0 ? DA_INIT_CAP : (da)->capacity*2;   \
	   (da)->items = realloc((da)->items, (da)->capacity*sizeof(*(da)->items)); \
	   assert((da)->items != NULL && "ERROR: No free RAM available");           \
       }                                                                            \
       (da)->items[(da)->count++] = (item);                                         \
       (da)->total++;                                                               \
   } while(0)                                                                       \

#define da_free(da) free((da).items)

/**
* @name Coroutine's context
*/
typedef struct
{
    void *rsp;         /**< Context's stack pointer.           */
    void *stack_base;  /**< Pointer to Coroutine's stack base. */
    int  id;           /**< Context's ID.                      */
    bool dead;         /**< Context finished?                  */
} Context;

/**
* @name Dynamic array (da) of Contexts
*/
typedef struct
{
    Context *items;   /**< Pointer to our Contexts.             */
    size_t total;     /**< Total amount of inserted Contexts.   */
    size_t count;     /**< Current number of inserted Contexts. */
    size_t capacity;  /**< Dynamic array current capacity.      */
    size_t current;   /**< Current running coroutine.           */
} Contexts;

Contexts contexts = {0};

void __attribute__((naked)) coroutine_yield(void)
{
    asm(
    "    pushq %rdi\n"
    "    pushq %rbx\n"
    "    pushq %rbp\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rsp, %rdi\n"
    "    jmp coroutine_switch_context\n"
    );
}

void __attribute__((naked)) coroutine_restore_context(void *rsp)
{
    (void)rsp;
    asm(
    "    movq %rdi, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbp\n"
    "    popq %rbx\n"
    "    popq %rdi\n"
    "    ret\n"
    );
}

void coroutine_switch_context(void *rsp)
{
    // Saving return address
    contexts.items[contexts.current].rsp = rsp;
    contexts.current = (contexts.current + 1) % contexts.count;
    coroutine_restore_context(contexts.items[contexts.current].rsp);
}

void coroutine_init(void)
{
    // No need to save the current rsp when initializing,
    // since next "yield" will update de rsp
    da_append(&contexts, ((Context){0}));
}

void coroutine_finish(void)
{
    if ( contexts.current == 0 ) {
	da_free(contexts);
	return;
    }

    // Currently, coroutines' stack are not being freed, we set them as dead.
    // By setting coroutines as dead, we won't need to free the stack. Whenever we allocate a new Coroutine,
    // instead of appending into Contexts, iterate through all Contexts and see if there's one dead ( preallocated )
    contexts.items[contexts.current].dead = true;
    // Whenever a coroutine finished, we must shift all the contexts to prevent holes in
    // ^ Using memmove to keep the same coroutine order
    memmove(
       &contexts.items[contexts.current],
       &contexts.items[contexts.current + 1],
       sizeof(*contexts.items)*(contexts.count - contexts.current - 1)
    );
    contexts.count -= 1;
    contexts.current %= contexts.count;
    coroutine_restore_context(contexts.items[contexts.current].rsp);
}

void coroutine_go(void (*f)(void*), void *arg)
{
    bool   found_free_stack = false;
    void   *stack_base;
    size_t found_index = 0;
    // Re-using the preallocated stacks
    if ( contexts.count > 1 )
    {
	for ( size_t i = contexts.count; i < contexts.capacity; i++ )
	{
	    if ( (found_free_stack = contexts.items[i].dead) ) { found_index = i; i = contexts.capacity; }
	}
	stack_base = contexts.items[found_index].stack_base;
    }

    if ( !found_free_stack ) stack_base = malloc(STACK_CAPACITY);

    //            /-> Fixing rsp since stack grows right-to-left
    //            v   Casting to CHAR makes the sum BYTE-wise
    void **rsp = (void**)((char*)stack_base + STACK_CAPACITY);
    // Pushing to stack the address to where the coroutine must return whenever it has finished
    *(--rsp) = coroutine_finish;
    // Pushing to stack the coroutine's return address ("f") and it's registers (following musl's setjmp)
    *(--rsp) = f;   // push ret address
    *(--rsp) = arg; // push rdi (Coroutines might have arguments)
    *(--rsp) = 0;   // push rbx
    *(--rsp) = 0;   // push rbp
    *(--rsp) = 0;   // push r12
    *(--rsp) = 0;   // push r13
    *(--rsp) = 0;   // push r14
    *(--rsp) = 0;   // push r15

    if ( !found_free_stack )
    {
       da_append(&contexts, ((Context){
       .rsp = rsp,
       .stack_base = stack_base,
       .id = contexts.total,
       .dead = false,
       }));
    }
    else
    {
       contexts.items[found_index].rsp = rsp;
       contexts.items[found_index].stack_base = stack_base;
       contexts.items[found_index].id = contexts.total;
       contexts.items[found_index].dead = false;
    }
}

size_t coroutine_id(void)
{
    // "MAIN" coroutine will always be id = 0
    return contexts.items[contexts.current].id;
}

size_t coroutines_alive(void)
{
    return contexts.count;
}
