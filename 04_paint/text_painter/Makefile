CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -I.

SRCS = main.cc text_painter.cc json_parser.cc \
       decoration_line_painter.cc text_decoration_info.cc text_decoration_painter.cc
OBJS = $(SRCS:.cc=.o)
TARGET = text_painter

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
