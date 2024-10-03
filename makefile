CC=clang
CXX=clang++
LLVM_CONFIG=llvm-config

# -DDEBUG
CXXFLAGS=$(shell $(LLVM_CONFIG) --cxxflags) -fPIC -Wall -Wextra -gdwarf-4
CFLAGS=$(shell $(LLVM_CONFIG) --cflags) -fPIC -Wall -Wextra -gdwarf-4
LDFLAGS=$(shell $(LLVM_CONFIG) --ldflags)
LIBS=$(shell $(LLVM_CONFIG) --libs core irreader)

SOURCES := $(shell find ./src/ -name "*.c" -o -name "*.cpp")

# Replace .c and .cpp extensions with .o for object files
OBJECTS := $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.cpp=.o)
EXECUTABLE=out/mel

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(LIBS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(EXECUTABLE) $(OBJECTS) out.ll test test.o

.PHONY: clean

run:
	./out/mel tests/test.mel test
	./test

install:
	sudo mkdir -p /usr/mel/include
	sudo mkdir -p /usr/mel/lib
	sudo cp lib/*.mh /usr/mel/include
	sudo cp lib/lib.a /usr/mel/lib/libstd.a
	sudo cp out/mel /usr/bin