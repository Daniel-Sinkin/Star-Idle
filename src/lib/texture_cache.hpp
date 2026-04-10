#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace star_idle::platform
{

struct TextureHandle
{
    SDL_Texture* texture{nullptr};
    int width{0};
    int height{0};
    std::filesystem::path source{};
};

class TextureCache
{
  public:
    TextureCache() = default;
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;
    ~TextureCache();

    [[nodiscard]] const TextureHandle* load(
        SDL_Renderer* renderer, const std::filesystem::path& path, std::string* error_out = nullptr
    );

    void clear();

  private:
    std::unordered_map<std::string, TextureHandle> textures_{};
};

}  // namespace star_idle::platform
