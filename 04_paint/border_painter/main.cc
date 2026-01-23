#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "border_painter.h"
#include "json_parser.h"

void PrintUsage(const char* program) {
  std::cerr << "Usage: " << program << " -i <input.json> [-o <output.json>]\n";
  std::cerr << "\n";
  std::cerr << "Options:\n";
  std::cerr << "  -i <file>  Input JSON file (required)\n";
  std::cerr << "  -o <file>  Output JSON file (default: stdout)\n";
}

int main(int argc, char* argv[]) {
  std::string input_file;
  std::string output_file;

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-i" && i + 1 < argc) {
      input_file = argv[++i];
    } else if (arg == "-o" && i + 1 < argc) {
      output_file = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      PrintUsage(argv[0]);
      return 0;
    }
  }

  if (input_file.empty()) {
    PrintUsage(argv[0]);
    return 1;
  }

  // Read input file
  std::ifstream ifs(input_file);
  if (!ifs) {
    std::cerr << "Error: Cannot open input file: " << input_file << "\n";
    return 1;
  }

  std::stringstream buffer;
  buffer << ifs.rdbuf();
  std::string json_input = buffer.str();

  // Parse input
  border_painter::BorderPaintInput input;
  try {
    input = border_painter::ParseInput(json_input);
  } catch (const std::exception& e) {
    std::cerr << "Error parsing input: " << e.what() << "\n";
    return 1;
  }

  // Paint borders
  auto ops = border_painter::BorderPainter::Paint(input);

  // Serialize output
  std::string json_output = border_painter::SerializeOps(ops);

  // Write output
  if (output_file.empty()) {
    std::cout << json_output;
  } else {
    std::ofstream ofs(output_file);
    if (!ofs) {
      std::cerr << "Error: Cannot open output file: " << output_file << "\n";
      return 1;
    }
    ofs << json_output;
  }

  return 0;
}
