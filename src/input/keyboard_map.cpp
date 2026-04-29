#include "piano/input/keyboard_map.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace piano::input {
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

bool KeyboardMap::LoadFromFile(const std::string& path, std::string* error_message) {
  std::ifstream input(path);
  if (!input.is_open()) {
    if (error_message != nullptr) {
      *error_message = "failed to open keyboard map file: " + path;
    }
    return false;
  }

  key_to_midi_.clear();
  std::size_t line_number = 0;
  std::string line;
  while (std::getline(input, line)) {
    ++line_number;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || IsComment(trimmed)) {
      continue;
    }

    std::istringstream line_stream(trimmed);
    std::string key_name;
    int midi_key = -1;
    if (!(line_stream >> key_name >> midi_key)) {
      if (error_message != nullptr) {
        *error_message = "invalid keyboard map line " + std::to_string(line_number) + ": " + trimmed;
      }
      return false;
    }

    if (midi_key < 0 || midi_key > 127) {
      if (error_message != nullptr) {
        *error_message = "midi key out of range at line " + std::to_string(line_number);
      }
      return false;
    }

    key_to_midi_[key_name] = midi_key;
  }

  return true;
}

std::optional<int> KeyboardMap::LookupMidiKey(const std::string& key_name) const {
  const auto it = key_to_midi_.find(key_name);
  if (it == key_to_midi_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::size_t KeyboardMap::Size() const {
  return key_to_midi_.size();
}

}  // namespace piano::input
