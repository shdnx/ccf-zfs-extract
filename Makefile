CXX = g++
CXXFLAGS = -DDEBUG -std=c++14 -ggdb -O0 -Wall -Wextra
INCLUDES = -Iinclude -Ideps/lz4xx/include
LDFLAGS = -Ldeps/lz4xx/build -Wl,-whole-archive -l:liblz4xx.a -Wl,-no-whole-archive

SRCS = $(wildcard src/*.cpp)
OBJS = $(patsubst src/%.cpp,obj/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)
BIN = bin/ccf-zfs

.PHONY: clean directories

all: directories $(BIN)

directories:
	mkdir -p obj
	mkdir -p bin

clean:
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f $(BIN)

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -MMD $< -o $@

$(BIN): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

-include $(DEPS)
