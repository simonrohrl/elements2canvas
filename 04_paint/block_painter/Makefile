CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Isrc

SRCDIR = src
BUILDDIR = build

SRCS = $(SRCDIR)/main.cc $(SRCDIR)/block_painter.cc $(SRCDIR)/json_parser.cc
OBJS = $(patsubst $(SRCDIR)/%.cc,$(BUILDDIR)/%.o,$(SRCS))
TARGET = $(BUILDDIR)/block_painter

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
