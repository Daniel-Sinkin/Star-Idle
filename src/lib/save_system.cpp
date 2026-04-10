#include "save_system.hpp"

#include <SDL3/SDL_filesystem.h>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <utility>

namespace star_idle::platform
{

SaveSystem::SaveSystem(std::string organization, std::string application_name)
    : organization_(std::move(organization)), application_name_(std::move(application_name))
{
    assign_save_path();
}

const std::filesystem::path& SaveSystem::save_path() const noexcept
{
    return save_path_;
}

const std::string& SaveSystem::last_error() const noexcept
{
    return last_error_;
}

bool SaveSystem::save(const game::GameState& state)
{
    if (save_path_.empty())
    {
        assign_save_path();
    }

    std::ofstream output(save_path_);
    if (!output.is_open())
    {
        last_error_ = "Unable to open save file for writing: " + save_path_.string();
        return false;
    }

    const nlohmann::json json = state;
    output << std::setw(2) << json << '\n';
    last_error_.clear();
    return true;
}

std::optional<game::GameState> SaveSystem::load() const
{
    if (save_path_.empty())
    {
        last_error_ = "Save path is not initialized.";
        return std::nullopt;
    }

    if (!std::filesystem::exists(save_path_))
    {
        last_error_.clear();
        return std::nullopt;
    }

    std::ifstream input(save_path_);
    if (!input.is_open())
    {
        last_error_ = "Unable to open save file for reading: " + save_path_.string();
        return std::nullopt;
    }

    try
    {
        nlohmann::json json;
        input >> json;
        last_error_.clear();
        return json.get<game::GameState>();
    }
    catch (const std::exception& exception)
    {
        last_error_ = exception.what();
        return std::nullopt;
    }
}

void SaveSystem::assign_save_path()
{
    char* pref_path = SDL_GetPrefPath(organization_.c_str(), application_name_.c_str());
    if (pref_path == nullptr)
    {
        save_path_ = std::filesystem::current_path() / "savegame.json";
        last_error_ = "SDL_GetPrefPath failed, falling back to local savegame.json.";
        return;
    }

    save_path_ = std::filesystem::path(pref_path) / "savegame.json";
    SDL_free(pref_path);
    last_error_.clear();
}

}  // namespace star_idle::platform
