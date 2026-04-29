#include "piano/score/score_parser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
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
    ScoreCommand command;
    if (!(line_stream >> command.begin_beats >> command.token >> command.last_beats >> command.value)) {
      if (error_message != nullptr) {
        *error_message = "invalid score line " + std::to_string(line_number) + ": " + trimmed;
      }
      return false;
    }

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
