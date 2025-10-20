CC = gcc
CFLAGS = -Wall -g -I.
LD = gcc
LDFLAGS = -Wall -g
PROGS = snakes nums hungry
SNAKEOBJS = randomsnakes.o
HUNGRYOBJS = hungrysnakes.o
NUMOBJS = numbersmain.o
OBJS = $(SNAKEOBJS) $(HUNGRYOBJS) $(NUMOBJS)
EXTRACLEAN = core $(PROGS) *.o *.a

all: $(PROGS)

allclean: clean
	@rm -f $(EXTRACLEAN)

clean:
	rm -f $(OBJS) *~ TAGS

snakes: randomsnakes.o libLWP.a libsnakes.a
	$(LD) $(LDFLAGS) -o snakes randomsnakes.o -L. -Wl,-Bstatic -lsnakes -Wl,-Bdynamic -lLWP -lncurses

hungry: hungrysnakes.o libLWP.a libsnakes.a
	$(LD) $(LDFLAGS) -o hungry hungrysnakes.o -L. -Wl,-Bstatic -lsnakes -Wl,-Bdynamic -lLWP -lncurses

nums: numbersmain.o libLWP.a
	$(LD) $(LDFLAGS) -o nums numbersmain.o -L. -lLWP

hungrysnakes.o: hungrysnakes.c lwp.h snakes.h
	$(CC) $(CFLAGS) -c hungrysnakes.c

randomsnakes.o: randomsnakes.c lwp.h snakes.h
	$(CC) $(CFLAGS) -c randomsnakes.c

numbersmain.o: numbersmain.c lwp.h
	$(CC) $(CFLAGS) -c numbersmain.c

libLWP.a: lwp.o rr.o util.o magic64.o
	ar r libLWP.a lwp.o rr.o util.o magic64.o

lwp.o: lwp.c lwp.h fp.h
	$(CC) $(CFLAGS) -c lwp.c

rr.o: rr.c lwp.h
	$(CC) $(CFLAGS) -c rr.c

util.o: util.c lwp.h
	$(CC) $(CFLAGS) -c util.c

magic64.o: magic64.S
	$(CC) $(CFLAGS) -c magic64.S

submission: lwp.c rr.c util.c Makefile README
	tar -cf project2_submission.tar lwp.c rr.c util.c Makefile README
	gzip project2_submission.tar
