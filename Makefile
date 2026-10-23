# Makefile — dependency-free fallback build (CMake is the primary build system).
#
#   make            build static library, CLI and tests
#   make test       run the unit tests
#   make clean
#
# Works with GCC/Clang on Linux, macOS and MinGW (mingw32-make).

CXX      ?= g++
CXXSTD   ?= -std=c++14
OPT      ?= -O2
WARN     ?= -Wall -Wextra -Wpedantic
BUILD    ?= build
INCLUDES := -Iinclude

# 32-bit x86 GCC needs SSE math for bit-exact results (x87 excess precision).
MACHINE := $(shell $(CXX) -dumpmachine 2>/dev/null)
ifneq (,$(findstring mingw32,$(MACHINE)))
  ifeq (,$(findstring x86_64,$(MACHINE)))
    ARCHFLAGS := -mfpmath=sse -msse2
  endif
endif
ifneq (,$(findstring i686,$(MACHINE)))
  ARCHFLAGS := -mfpmath=sse -msse2
endif

CXXFLAGS := $(CXXSTD) $(OPT) $(WARN) $(ARCHFLAGS) $(INCLUDES)

ifeq ($(OS),Windows_NT)
  EXE := .exe
  RM  := cmd /C del /Q /S
else
  EXE :=
  RM  := rm -rf
endif

LIB_SRCS := $(wildcard src/*.cpp)
LIB_SRCS := $(filter-out src/wasm_entry.cpp,$(LIB_SRCS))
LIB_OBJS := $(patsubst src/%.cpp,$(BUILD)/obj/%.o,$(LIB_SRCS))
TEST_SRCS := $(wildcard tests/*.cpp)
TEST_OBJS := $(patsubst tests/%.cpp,$(BUILD)/obj/tests/%.o,$(TEST_SRCS))

LIB := $(BUILD)/libcml.a
CLI := $(BUILD)/cml-cli$(EXE)
TESTS := $(BUILD)/cml-tests$(EXE)

all: $(LIB) $(CLI) $(TESTS)

$(BUILD)/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@) 2>/dev/null || true
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/obj/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@) 2>/dev/null || true
	$(CXX) $(CXXFLAGS) -Itests -c $< -o $@

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

$(CLI): apps/cli/main.cpp $(LIB)
	$(CXX) $(CXXFLAGS) $< $(LIB) -o $@

$(TESTS): $(TEST_OBJS) $(LIB)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(LIB) -o $@

test: $(TESTS)
	$(TESTS)

clean:
	-$(RM) $(BUILD)

.PHONY: all test clean
