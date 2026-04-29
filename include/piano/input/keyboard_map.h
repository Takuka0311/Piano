#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace piano::input {

class KeyboardMap {
 public:
  bool LoadFromFile(const std::string& path, std::string* error_message);
  std::optional<int> LookupMidiKey(const std::string& key_name) const;
  std::size_t Size() const;

 private:
  std::unordered_map<std::string, int> key_to_midi_;
};

}  // namespace piano::input
