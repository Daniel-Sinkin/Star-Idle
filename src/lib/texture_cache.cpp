#include "texture_cache.hpp"

#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <filesystem>
#include <stb_image.h>

namespace star_idle::platform
{

TextureCache::~TextureCache()
{
    clear();
}

const TextureHandle* TextureCache::load(
    SDL_Renderer* renderer, const std::filesystem::path& path, std::string* error_out
)
{
    const std::string key = path.lexically_normal().string();
    if (const auto cached = textures_.find(key); cached != textures_.end())
    {
        if (error_out != nullptr)
        {
            error_out->clear();
        }
        return &cached->second;
    }

    if (!std::filesystem::exists(path))
    {
        if (error_out != nullptr)
        {
            *error_out = "Image pipeline ready. Add assets/images/sector_preview.png when you want "
                         "art in the UI.";
        }
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int channel_count = 0;
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channel_count, 4);
    if (pixels == nullptr)
    {
        if (error_out != nullptr)
        {
            *error_out = stbi_failure_reason() != nullptr ? stbi_failure_reason()
                                                          : "stb_image failed to load texture.";
        }
        return nullptr;
    }

    SDL_Surface* surface =
        SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
    if (surface == nullptr)
    {
        if (error_out != nullptr)
        {
            *error_out = SDL_GetError();
        }
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    stbi_image_free(pixels);
    if (texture == nullptr)
    {
        if (error_out != nullptr)
        {
            *error_out = SDL_GetError();
        }
        return nullptr;
    }

    auto [iterator, inserted] = textures_.emplace(
        key,
        TextureHandle{
            .texture = texture,
            .width = width,
            .height = height,
            .source = path,
        }
    );
    (void) inserted;

    if (error_out != nullptr)
    {
        error_out->clear();
    }
    return &iterator->second;
}

void TextureCache::clear()
{
    for (auto& [key, texture] : textures_)
    {
        (void) key;
        if (texture.texture != nullptr)
        {
            SDL_DestroyTexture(texture.texture);
            texture.texture = nullptr;
        }
    }
    textures_.clear();
}

}  // namespace star_idle::platform
