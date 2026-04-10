#pragma once

#include "common.hpp"
#include "game_state.hpp"
#include "save_system.hpp"
#include "texture_cache.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <string>

namespace star_idle
{

class Application
{
  public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run();

  private:
    bool initialize();
    void shutdown();
    void poll_events();
    void update(f64 delta_seconds);
    void render(f64 delta_seconds);
    void render_dockspace();
    void build_default_layout();
    void render_sector_overview_window();
    void render_economy_window();
    void render_systems_window();
    void render_command_window();
    void render_telemetry_window();
    void try_load_preview_texture();

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};

    platform::TextureCache texture_cache_{};
    platform::SaveSystem save_system_{"Daniel Sinkin", "Star Idle"};
    game::GameState game_state_{};

    std::filesystem::path base_path_{};
    std::filesystem::path preview_texture_path_{};
    const platform::TextureHandle* preview_texture_{nullptr};

    bool running_{true};
    bool paused_{false};
    bool dock_layout_built_{false};
    bool show_imgui_demo_{false};
    bool attempted_preview_load_{false};

    std::string last_status_{"Bootstrapped sector command."};
    std::string renderer_name_{"unknown"};
    f64 smoothed_frame_ms_{16.0};
};

}  // namespace star_idle
