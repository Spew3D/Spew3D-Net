
ifneq (,$(findstring mingw,$(CC)))
BINEXT:=.exe
else
BINEXT:=.bin
endif
UNITTEST_SOURCES_NOSDL=$(sort $(wildcard ./implementation/test_*_nosdl.c))
UNITTEST_SOURCES=$(sort $(wildcard ./implementation/test_*.c))
UNITTEST_SOURCES_WITHSDL=$(sort $(filter-out $(UNITTEST_SOURCES), $(UNITTEST_SOURCES_NOSDL)))
UNITTEST_BASENAMES=$(sort $(patsubst %.c, %, $(UNITTEST_SOURCES)))
UNITTEST_BASENAMES_NOSDL=$(sort $(patsubst %.c, %, $(UNITTEST_SOURCES_NOSDL)))
UNITTEST_BASENAMES_WITHSDL=$(sort $(patsubst %.c, %, $(UNITTEST_SOURCES_WITHSDL)))
HEADERS=$(sort $(filter-out ./include/spew3dnet.h ./implementation/testmain.h ./implementation/spew3dnet_prefix_all.h,$(wildcard ./include/*.h) $(wildcard ./implementation/*.h)))
SOURCES=$(sort $(filter-out $(UNITTEST_SOURCES), $(wildcard ./implementation/*.c)))
TESTPROG=$(sort $(patsubst %.c, %$(BINEXT), $(wildcard ./examples/example_*.c ./implementation/test_*.c)))

all: amalgamate build-tests

amalgamate: update-vendor-if-needed
	echo "#ifdef SPEW3DNET_IMPLEMENTATION" > .spew3d_ifdef
	echo "" >> .spew3d_ifdef
	echo "#endif  // SPEW3DNET_IMPLEMENTATION" > .spew3d_ifndef
	echo "" >> .spew3d_ifndef
	cat implementation/spew3dnet_prefix_all.h $(HEADERS) $(SOURCES) > include/spew3dnet.h
	rm -f .spew3d_ifdef
	rm -f .spew3d_ifndef

build-tests:
	cd examples && $(MAKE) clean && $(MAKE) CC="$(CC)"

test: amalgamate build-tests unittests
	cd examples && valgrind ./example_markdown_basic.bin

update-vendor-if-needed:
	@if [ ! -e "vendor/Spew3D/include/spew3d.h" ]; then $(MAKE) update-vendor; fi

update-vendor:
	@if [ ! -e "vendor/Spew3D/AUTHORS.md" ]; then git submodule update --init; fi
	cd vendor/Spew3D/ && make amalgamate
	cd vendor/Spew3D-Web/ && make amalgamate

unittests:
	echo "TESTS: $(UNITTEST_SOURCES) | $(UNITTEST_BASENAMES)"
	for x in $(UNITTEST_BASENAMES_WITHSDL); do $(CC) -g -O0 $(CFLAGS) -Iinclude/ -I./vendor/Spew3D/include/ $(CXXFLAGS) -pthread -o ./$$x$(BINEXT) ./$$x.c -lSDL2 -lcheck -lrt -lm $(LDFLAGS) || { exit 1; }; done
	for x in $(UNITTEST_BASENAMES_NOSDL); do $(CC) -g -O0 $(CFLAGS) -Iinclude/ -I./vendor/Spew3D/include/ $(CXXFLAGS) -pthread -o ./$$x$(BINEXT) ./$$x.c -lcheck -lrt -lm $(LDFLAGS) || { exit 1; }; done
	for x in $(UNITTEST_BASENAMES); do echo ">>> TEST RUN: $$x"; CK_FORK=no valgrind --track-origins=yes --leak-check=full ./$$x$(BINEXT) || { exit 1; }; done

clean:
	rm -f $(TESTPROG)
	rm -f ./include/spew3dnet.h

