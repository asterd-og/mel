CC=clang
CXX=clang++
LLVM_CONFIG=llvm-config

CXXFLAGS=$(shell $(LLVM_CONFIG) --cxxflags) -DDEBUG -fPIC -g
CFLAGS=$(shell $(LLVM_CONFIG) --cflags) -DDEBUG -fPIC -g
LDFLAGS=$(shell $(LLVM_CONFIG) --ldflags)
LIBS=$(shell $(LLVM_CONFIG) --libs core irreader)

SOURCES := $(shell find . -name "*.c" -o -name "*.cpp")

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
	./out/mel tests/helloworld.mel test

comp:
	llc -filetype=obj out.ll -o test.o -opaque-pointers
	clang test.o lib/mlib.a -o test
	./test

compAsm:
	llc out.ll -o test.s