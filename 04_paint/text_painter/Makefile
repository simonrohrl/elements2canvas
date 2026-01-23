CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Isrc

SRCDIR = src
BUILDDIR = build

SRCS = $(SRCDIR)/main.cc $(SRCDIR)/text_painter.cc $(SRCDIR)/json_parser.cc \
       $(SRCDIR)/decoration_line_painter.cc $(SRCDIR)/text_decoration_info.cc \
       $(SRCDIR)/text_decoration_painter.cc
OBJS = $(patsubst $(SRCDIR)/%.cc,$(BUILDDIR)/%.o,$(SRCS))
TARGET = $(BUILDDIR)/text_painter

all: $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.cc | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)

run: $(TARGET)
	./$(TARGET) -i test/input.json

.PHONY: all clean run
