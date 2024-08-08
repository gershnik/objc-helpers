
CLANG ?= clang++
SOURCES:=BlockUtilTestCpp.cpp CoDispatchTestsCpp.cpp main-linux.cpp
CPPFLAGS:=--std=c++20 -fblocks -I../include
LDFLAGS:=--std=c++20 -fblocks -ldispatch -lBlocksRuntime

.DEFAULT_GOAL:=build/test

build:
	mkdir $@

build/main-linux.o: main-linux.cpp ../include/objc-helpers/BlockUtil.h build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/BlockUtilTestCpp.o: BlockUtilTestCpp.cpp ../include/objc-helpers/BlockUtil.h build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/CoDispatchTestsCpp.o: CoDispatchTestsCpp.cpp ../include/objc-helpers/CoDispatch.h build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/CoDispatchTestsNoexcept.o: CoDispatchTestsNoexcept.cpp ../include/objc-helpers/CoDispatch.h build
	$(CLANG) $(CPPFLAGS) -c -o $@  $<

build/test: build/main-linux.o \
			build/BlockUtilTestCpp.o \
			build/CoDispatchTestsCpp.o \
			build/CoDispatchTestsNoexcept.o
	$(CLANG) $(LDFLAGS) -o $@ $^
