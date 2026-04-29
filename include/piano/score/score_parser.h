#pragma once

#include <string>
#include <vector>

namespace piano::score {

struct ScoreCommand {
  double begin_beats = 0.0;
  std::string token;
  double last_beats = 0.0;
  int value = 0;
};

class ScoreParser {
 public:
  bool LoadFromFile(const std::string& path, std::string* error_message);
  const std::vector<ScoreCommand>& Commands() const;

 private:
  std::vector<ScoreCommand> commands_;
};

}  // namespace piano::score
