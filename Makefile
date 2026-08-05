CXX      = g++
BUILD   ?= release

BASE_CXXFLAGS = -std=c++20 \
                -Wall \
                -Wextra \
                -ffunction-sections \
                -fdata-sections

ifeq ($(BUILD),debug)
    CXXFLAGS = $(BASE_CXXFLAGS) -g -O0 -DDEBUG_BUILD
    LDFLAGS  =
else
    CXXFLAGS = $(BASE_CXXFLAGS) -O3 -flto -DNDEBUG
    LDFLAGS  = -flto
endif

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    ifeq ($(BUILD),release)
        LDFLAGS += -Wl,-dead_strip
    endif
else
    LDFLAGS += -Wl,--gc-sections
    ifeq ($(BUILD),release)
        LDFLAGS += -Wl,-s
    endif
endif

SRC_DIR = src

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:.cpp=.o)

CXXFLAGS += -I$(SRC_DIR)

LUA_SCRIPT = $(SRC_DIR)/succubid_selector.lua
LUA_HEADER = $(SRC_DIR)/succubid_selector.hpp

ifeq ($(UNAME_S),Darwin)
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

    ifneq ($(BREW_PREFIX),)
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS  += -L$(BREW_PREFIX)/lib

        REAL_GPP := $(lastword $(sort $(wildcard $(BREW_PREFIX)/bin/g++-[0-9]*)))

        ifneq ($(REAL_GPP),)
            CXX := $(REAL_GPP)
        endif
    endif
endif

ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lrt
endif

LDFLAGS += -lcurl

TARGET = succubid

all: $(TARGET)
	@echo "Summoned a Succubid [$(BUILD) mode] using: $$($(CXX) --version | head -n 1)"

debug:
	@$(MAKE) BUILD=debug clean-objs $(TARGET)

valgrind: debug
	valgrind --leak-check=full --track-origins=yes ./$(TARGET) -k test -gl

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)
ifeq ($(UNAME_S),Darwin)
ifeq ($(BUILD),release)
	strip $(TARGET)
endif
endif

$(LUA_HEADER): $(LUA_SCRIPT)
	xxd -i $(LUA_SCRIPT) | \
	sed 's/src_succubid_selector_lua/succubid_selector_lua/g' > $(LUA_HEADER)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp $(wildcard $(SRC_DIR)/*.h) $(LUA_HEADER)
	$(CXX) $(CXXFLAGS) -c $< -o $@

FORMAT_FILES = $(shell find src -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))

format:
	astyle \
		--style=google \
		--indent=force-tab=4 \
		--indent-switches \
		--pad-oper \
		--unpad-paren \
		--align-pointer=type \
		--lineend=linux \
		--suffix=none \
		$(FORMAT_FILES)
	stylua $(SRC_DIR)/*.lua

clean-objs:
	rm -f $(OBJS)

clean: clean-objs
	rm -f $(TARGET) $(LUA_HEADER)

.PHONY: all debug clean clean-objs format valgrind
