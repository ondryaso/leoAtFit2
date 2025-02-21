CC = /usr/local/bin/g++-12.3
CFLAGS = -Wall -g -std=c++17 -O2

LD = $(CC)
LDFLAGS = -g

# The libraries to link
LIBS = -lopencv_core

# The sources to compile
SOURCES = $(wildcard *.cpp)

# The objects to compile
OBJECTS = $(SOURCES:.cpp=.o)

# The executable to create
EXECUTABLE = bms

# The default target
all: $(EXECUTABLE)

# The executable target
$(EXECUTABLE): $(OBJECTS)
	$(LD) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

# The object targets
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# The clean target
clean:
	rm -f $(EXECUTABLE) $(OBJECTS)

# The run target
run: $(EXECUTABLE)
	./$(EXECUTABLE)
