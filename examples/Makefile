

ifneq (,$(findstring mingw,$(CC)))
BINEXT:=.exe
else
BINEXT:=.bin
endif
TESTPROG=$(sort $(patsubst %.c, %$(BINEXT), $(wildcard ./example_*.c)))

build: amalgamate $(TESTPROG)

amalgamate:
	cd ../ && $(MAKE) amalgamate

example_%$(BINEXT): example_%.c
	$(CC) -pthread -g -O0 -o $@ $(patsubst %$(BINEXT),%.c, $@) -I../include -I../vendor/Spew3D-Web/include/ -I../vendor/Spew3D/include/ -lm -lSDL2

clean:
	rm -rf $(TESTPROG)
