#include "piano/score/score_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>

namespace piano::score {
namespace {

std::string Trim(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(begin, end - begin);
}

bool IsComment(const std::string& line) {
  return line.rfind("#", 0) == 0 || line.rfind("//", 0) == 0;
}

bool TryParseDouble(const std::string& value, double* out) {
  if (out == nullptr) {
    return false;
  }
  char* end_ptr = nullptr;
  const double parsed = std::strtod(value.c_str(), &end_ptr);
  if (end_ptr == nullptr || *end_ptr != '\0') {
    return false;
  }
  if (parsed == std::numeric_limits<double>::infinity() || parsed == -std::numeric_limits<double>::infinity()) {
    return false;
  }
  *out = parsed;
  return true;
}

bool TryParseInt(const std::string& value, int* out) {
  if (out == nullptr) {
    return false;
  }
  char* end_ptr = nullptr;
  const long parsed = std::strtol(value.c_str(), &end_ptr, 10);
  if (end_ptr == nullptr || *end_ptr != '\0') {
    return false;
  }
  *out = static_cast<int>(parsed);
  return true;
}

bool IsLikelyToken(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  if (token == "0" || token == "S") {
    return true;
  }
  if (std::isalpha(static_cast<unsigned char>(token[0])) != 0) {
    const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (up >= 'A' && up <= 'G') {
      if (token.size() == 1) {
        return true;
      }
      if (token.size() == 2 && (token[1] == '+' || token[1] == '-')) {
        return true;
      }
    }
  }
  if (token[0] < '1' || token[0] > '7') {
    return false;
  }
  std::size_t pos = 1;
  if (pos < token.size() && token[pos] == '#') {
    ++pos;
  }
  while (pos < token.size() && (token[pos] == '+' || token[pos] == '-')) {
    ++pos;
  }
  return pos == token.size();
}

}  // namespace

bool ScoreParser::LoadFromFile(const std::string& path, std::string* error_message) {
  std::ifstream input(path);
  if (!input.is_open()) {
    if (error_message != nullptr) {
      *error_message = "failed to open score file: " + path;
    }
    return false;
  }

  commands_.clear();
  std::size_t line_number = 0;
  std::string line;
  while (std::getline(input, line)) {
    ++line_number;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || IsComment(trimmed)) {
      continue;
    }

    std::istringstream line_stream(trimmed);
    std::string f1;
    std::string f2;
    std::string f3;
    std::string f4;
    ScoreCommand command;
    if (!(line_stream >> f1 >> f2 >> f3 >> f4)) {
      if (error_message != nullptr) {
        *error_message = "invalid score line " + std::to_string(line_number) + ": " + trimmed;
      }
      return false;
    }
    if (line_stream >> std::ws && !line_stream.eof()) {
      if (error_message != nullptr) {
        *error_message = "invalid score line " + std::to_string(line_number) + ": too many fields";
      }
      return false;
    }

    if (!TryParseDouble(f1, &command.begin_beats) || !TryParseInt(f4, &command.value)) {
      if (error_message != nullptr) {
        *error_message = "invalid score line " + std::to_string(line_number) + ": " + trimmed;
      }
      return false;
    }

    double duration = 0.0;
    const bool f2_is_token = IsLikelyToken(f2);
    const bool f3_is_token = IsLikelyToken(f3);
    if (f2_is_token && TryParseDouble(f3, &duration)) {
      command.token = f2;
      command.last_beats = duration;
    } else if (f3_is_token && TryParseDouble(f2, &command.last_beats)) {
      // Legacy format compatibility: begin last token db
      command.token = f3;
    } else if (TryParseDouble(f3, &duration) && !f3_is_token) {
      command.token = f2;
      command.last_beats = duration;
    } else {
      if (TryParseDouble(f2, &command.last_beats) && IsLikelyToken(f3)) {
        // Legacy format compatibility: begin last token db
        command.token = f3;
      } else {
        if (error_message != nullptr) {
          *error_message = "invalid score line " + std::to_string(line_number) + ": " + trimmed;
        }
        return false;
      }
    }
    command.source_line = line_number;
    command.source_text = trimmed;

    commands_.push_back(command);
  }

  std::stable_sort(commands_.begin(), commands_.end(),
                   [](const ScoreCommand& left, const ScoreCommand& right) {
                     return left.begin_beats < right.begin_beats;
                   });

  return true;
}

const std::vector<ScoreCommand>& ScoreParser::Commands() const {
  return commands_;
}

}  // namespace piano::score
