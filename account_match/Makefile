#BUILD_WINDOWS = 1

ifdef BUILD_WINDOWS
CC := i586-mingw32msvc-gcc
acc_match := account_match.exe
else
CC := gcc
acc_match := account_match
endif

SRCS := $(wildcard *.c)

OJB_FILES := acc_match.o parser.o sort.o bsearch.o xstrtod.o dos2unix.o

all: $(acc_match)

$(acc_match): $(OJB_FILES)
	$(CC) $(CFLAGS) -o $@ $^

.depend: Makefile $(SRCS)
	@$(CC) $(CFLAGS) -MM $(SRCS) >$@
	sinclude .depend

clean:
	rm -rf *.o .depend $(acc_match)


