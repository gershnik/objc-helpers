
CLANG ?= clang++
SOURCES:=BlockUtilTestCpp.cpp CoDispatchTestsCpp.cpp main-linux.cpp
CPPFLAGS:=--std=c++20 -fblocks -I../include -O2 -DNDEBUG
LDFLAGS:=--std=c++20 -fblocks -ldispatch -lBlocksRuntime

.DEFAULT_GOAL:=build/test

build:
	mkdir $@

build/main.o: main.mm ../include/objc-helpers/BlockUtil.h doctest.h build
	$(CLANG) $(CPPFLAGS) -c -o $@ --language 'c++' $<

build/TestGlobal.o: TestGlobal.cpp TestGlobal.h build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/BlockUtilTestCpp.o: BlockUtilTestCpp.cpp ../include/objc-helpers/BlockUtil.h doctest.h build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/CoDispatchTestsCpp.o: CoDispatchTestsCpp.cpp \
							../include/objc-helpers/CoDispatch.h \
							TestGlobal.h \
							doctest.h \
							build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/CoDispatchTestsNoexcept.o: CoDispatchTestsNoexcept.cpp \
							     ../include/objc-helpers/CoDispatch.h \
								 TestGlobal.h \
								 doctest.h \
								 build
	$(CLANG) $(CPPFLAGS) -fno-exceptions -c -o $@  $<

build/test: build/main.o \
			build/TestGlobal.o \
			build/BlockUtilTestCpp.o \
			build/CoDispatchTestsCpp.o \
			build/CoDispatchTestsNoexcept.o
	$(CLANG) $(LDFLAGS) -o $@ $^
