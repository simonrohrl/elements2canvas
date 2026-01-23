CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -I.

SRCS = main.cc block_painter.cc json_parser.cc
OBJS = $(SRCS:.cc=.o)
TARGET = block_painter

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) -i input.json

.PHONY: all clean run
