#include "json_parser.h"
#include "text_painter.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
  std::string input_file = "input.json";
  std::string output_file = "";

  // Parse command line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-i" && i + 1 < argc) {
      input_file = argv[++i];
    } else if (arg == "-o" && i + 1 < argc) {
      output_file = argv[++i];
    } else if (arg == "-h" || arg == "--help") {
      std::cout << "Usage: " << argv[0] << " [-i input.json] [-o output.json]\n";
      return 0;
    }
  }

  // Read input JSON
  std::ifstream ifs(input_file);
  if (!ifs) {
    std::cerr << "Error: Cannot open input file: " << input_file << "\n";
    return 1;
  }

  std::ostringstream buffer;
  buffer << ifs.rdbuf();
  std::string json_input = buffer.str();

  // Parse input
  TextPaintInput input;
  if (!JsonParser::ParseInput(json_input, input)) {
    std::cerr << "Error: Failed to parse input JSON\n";
    return 1;
  }

  // Run text painter
  PaintOpList ops = TextPainter::Paint(input);

  // Serialize output
  std::string json_output = JsonParser::SerializeOps(ops);

  // Write output
  if (output_file.empty()) {
    std::cout << json_output << "\n";
  } else {
    std::ofstream ofs(output_file);
    if (!ofs) {
      std::cerr << "Error: Cannot open output file: " << output_file << "\n";
      return 1;
    }
    ofs << json_output << "\n";
  }

  return 0;
}
