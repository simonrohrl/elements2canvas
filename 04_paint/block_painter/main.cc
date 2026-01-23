#include "block_painter.h"
#include "json_parser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

void PrintUsage(const char* program_name) {
  std::cerr << "Usage: " << program_name << " [options]\n"
            << "\n"
            << "Options:\n"
            << "  -i <file>    Input JSON file (default: input.json)\n"
            << "  -o <file>    Output JSON file (default: stdout)\n"
            << "  -h, --help   Show this help message\n";
}

std::string ReadFile(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file: " << path << std::endl;
    return "";
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

bool WriteFile(const std::string& path, const std::string& content) {
  std::ofstream file(path);
  if (!file.is_open()) {
    std::cerr << "Error: Could not write to file: " << path << std::endl;
    return false;
  }
  file << content;
  return true;
}

int main(int argc, char* argv[]) {
  std::string input_file = "input.json";
  std::string output_file;

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    } else if (arg == "-i" && i + 1 < argc) {
      input_file = argv[++i];
    } else if (arg == "-o" && i + 1 < argc) {
      output_file = argv[++i];
    } else {
      std::cerr << "Unknown option: " << arg << std::endl;
      PrintUsage(argv[0]);
      return 1;
    }
  }

  // Read input file
  std::string json_input = ReadFile(input_file);
  if (json_input.empty()) {
    return 1;
  }

  // Parse input
  BlockPaintInput input;
  if (!JsonParser::ParseInput(json_input, input)) {
    std::cerr << "Error: Failed to parse input JSON" << std::endl;
    return 1;
  }

  // Run block painter
  PaintOpList ops = BlockPainter::Paint(input);

  // Serialize output
  std::string json_output = JsonParser::SerializeOps(ops);

  // Write output
  if (output_file.empty()) {
    std::cout << json_output << std::endl;
  } else {
    if (!WriteFile(output_file, json_output)) {
      return 1;
    }
  }

  return 0;
}
