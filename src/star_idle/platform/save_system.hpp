#pragma once

#include "star_idle/game/game_state.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace star_idle::platform
{

class SaveSystem
{
  public:
    SaveSystem(std::string organization, std::string application_name);

    [[nodiscard]] const std::filesystem::path& save_path() const noexcept;
    [[nodiscard]] const std::string& last_error() const noexcept;

    bool save(const game::GameState& state);
    [[nodiscard]] std::optional<game::GameState> load() const;

  private:
    void assign_save_path();

    std::string organization_{};
    std::string application_name_{};
    std::filesystem::path save_path_{};
    mutable std::string last_error_{};
};

}  // namespace star_idle::platform
