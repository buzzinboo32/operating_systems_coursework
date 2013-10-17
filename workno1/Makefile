CC = gcc
FLAGS = -Wall

all: ut.a

ut.a: ut.c ut.h
	${CC} ${FLAGS} -c ut.c ut.h
	ar rcu libut.a ut.o
	ranlib libut.a

clean:
	rm -f *.o
	rm -f a.out
	rm -f *~
	rm -f ph
	rm -f *a
