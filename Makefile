C=		gcc
CFLAGS=		-g -gdwarf-2 -Wall -Werror -std=gnu99
LD=		gcc
LDFLAGS=	-L.
AR=		ar
ARFLAGS=	rcs
TARGETS=	spidey forking handler request single socket utils

all:		$(TARGETS)
	
test:
	@$(MAKE) -sk test-all

test-all:
	test-spidey

test-spidey:	spidey
	curl -sLO https://gitlab.com/nd-cse-20289-sp18/cse-20289-sp18-project/raw/master/test_spidey.sh
	chmod +x test_spidey.sh
	./test_spidey.sh

clean:
	@echo Cleaning...
	@rm -f $(TARGETS) *.o *.log *.input

.SUFFIXES:
%.o:				%.c
	$(CC) $(CFLAGS) -c -o $@ $<

spidey: forking.o handler.o request.o single.o socket.o spidey.o utils.o
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY:		all test benchmark clean
