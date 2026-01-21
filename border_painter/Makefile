CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SRCS = main.cc border_painter.cc json_parser.cc
OBJS = $(SRCS:.cc=.o)
TARGET = border_painter

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

main.o: main.cc border_painter.h json_parser.h types.h draw_commands.h
border_painter.o: border_painter.cc border_painter.h types.h draw_commands.h
json_parser.o: json_parser.cc json_parser.h border_painter.h types.h draw_commands.h

clean:
	rm -f $(OBJS) $(TARGET)
