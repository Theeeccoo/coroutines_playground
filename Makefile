main_new: main.c coroutine.o
	gcc -Wall -Wextra -ggdb -o main main.c coroutine.o

coroutine.o: coroutine.c
	gcc -Wall -Wextra -ggdb -c coroutine.c
