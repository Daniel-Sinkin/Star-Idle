#include "application.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>
#include <sstream>
#include <string>

namespace star_idle
{

namespace
{

constexpr const char* kWindowTitle = "Star Idle";
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr f64 kMaxDeltaSeconds = 0.25;

[[nodiscard]] std::string format_amount(f64 value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream.precision(value < 10.0 ? 2 : 1);
    stream << value;
    return stream.str();
}

[[nodiscard]] bool can_afford(const game::GameState& state, const game::StructureCost& cost)
{
    return state.credits >= cost.credits && state.alloys >= cost.alloys;
}

void apply_star_idle_style()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.TabRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.07f, 0.10f, 0.97f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.13f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.24f, 0.33f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.18f, 0.28f, 0.37f, 0.90f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.37f, 0.48f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.32f, 0.41f, 0.95f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.40f, 0.52f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.14f, 0.27f, 0.36f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.17f, 0.23f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.23f, 0.35f, 0.47f, 1.0f);
    style.Colors[ImGuiCol_TabSelected] = ImVec4(0.19f, 0.30f, 0.40f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.74f, 0.82f, 0.40f, 1.0f);
}

}  // namespace

Application::Application() = default;

Application::~Application()
{
    shutdown();
}

int Application::run()
{
    if (!initialize())
    {
        std::fprintf(stderr, "%s\n", last_status_.c_str());
        shutdown();
        return 1;
    }

    auto last_frame = std::chrono::steady_clock::now();
    while (running_)
    {
        poll_events();

        const auto now = std::chrono::steady_clock::now();
        f64 delta_seconds = std::chrono::duration<f64>(now - last_frame).count();
        last_frame = now;
        delta_seconds = std::clamp(delta_seconds, 0.0, kMaxDeltaSeconds);

        update(delta_seconds);
        render(delta_seconds);
    }

    shutdown();
    return 0;
}

bool Application::initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        last_status_ = std::string("SDL initialization failed: ") + SDL_GetError();
        return false;
    }

    window_ = SDL_CreateWindow(
        kWindowTitle,
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (window_ == nullptr)
    {
        last_status_ = std::string("Window creation failed: ") + SDL_GetError();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr)
    {
        last_status_ = std::string("Renderer creation failed: ") + SDL_GetError();
        return false;
    }

    if (!SDL_SetRenderVSync(renderer_, 1))
    {
        last_status_ = std::string("VSync request failed: ") + SDL_GetError();
    }

    if (const char* renderer_name = SDL_GetRendererName(renderer_); renderer_name != nullptr)
    {
        renderer_name_ = renderer_name;
    }

    if (const char* base_path = SDL_GetBasePath(); base_path != nullptr)
    {
        base_path_ = std::filesystem::path(base_path);
    }
    else
    {
        base_path_ = std::filesystem::current_path();
    }
    preview_texture_path_ = base_path_ / "assets" / "images" / "sector_preview.png";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    apply_star_idle_style();

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_))
    {
        last_status_ = "ImGui SDL3 backend initialization failed.";
        return false;
    }

    if (!ImGui_ImplSDLRenderer3_Init(renderer_))
    {
        last_status_ = "ImGui SDL_Renderer3 backend initialization failed.";
        return false;
    }

    if (const auto loaded_state = save_system_.load(); loaded_state.has_value())
    {
        game_state_ = *loaded_state;
        last_status_ = "Loaded previous sector state.";
    }
    else if (!save_system_.last_error().empty())
    {
        last_status_ = save_system_.last_error();
    }
    else
    {
        last_status_ = "Window initialized. Dockspace online.";
    }

    return true;
}

void Application::shutdown()
{
    texture_cache_.clear();

    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    if (renderer_ != nullptr)
    {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (SDL_WasInit(0U) != 0U)
    {
        SDL_Quit();
    }
}

void Application::poll_events()
{
    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event.window.windowID == SDL_GetWindowID(window_))
                {
                    running_ = false;
                }
                break;
            default:
                break;
        }
    }
}

void Application::update(f64 delta_seconds)
{
    smoothed_frame_ms_ = (smoothed_frame_ms_ * 0.9) + (delta_seconds * 1000.0 * 0.1);
    if (!paused_)
    {
        game::advance(game_state_, delta_seconds);
    }
}

void Application::render(f64 delta_seconds)
{
    (void) delta_seconds;

    try_load_preview_texture();

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    render_dockspace();
    render_sector_overview_window();
    render_economy_window();
    render_systems_window();
    render_command_window();
    render_telemetry_window();

    if (show_imgui_demo_)
    {
        ImGui::ShowDemoWindow(&show_imgui_demo_);
    }

    ImGui::Render();

    SDL_SetRenderDrawColor(renderer_, 10, 14, 22, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);
}

void Application::render_dockspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    constexpr ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("Star Idle Dockspace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save sector state"))
            {
                if (save_system_.save(game_state_))
                {
                    last_status_ = "Saved sector state.";
                }
                else
                {
                    last_status_ = save_system_.last_error();
                }
            }
            if (ImGui::MenuItem("Load sector state"))
            {
                if (const auto loaded = save_system_.load(); loaded.has_value())
                {
                    game_state_ = *loaded;
                    last_status_ = "Loaded sector state.";
                }
                else if (!save_system_.last_error().empty())
                {
                    last_status_ = save_system_.last_error();
                }
                else
                {
                    last_status_ = "No save file found yet.";
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                running_ = false;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Simulation"))
        {
            if (ImGui::MenuItem(paused_ ? "Resume" : "Pause"))
            {
                paused_ = !paused_;
                last_status_ = paused_ ? "Simulation paused." : "Simulation resumed.";
            }
            if (ImGui::MenuItem("Reset sector"))
            {
                game_state_ = {};
                paused_ = false;
                last_status_ = "Sector reset to bootstrap defaults.";
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("ImGui demo window", nullptr, &show_imgui_demo_);
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    const ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    if (!dock_layout_built_)
    {
        build_default_layout();
    }

    ImGui::End();
}

void Application::build_default_layout()
{
    dock_layout_built_ = true;

    const ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    ImGuiID main_id = dockspace_id;
    ImGuiID right_id =
        ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Right, 0.28f, nullptr, &main_id);
    ImGuiID bottom_id =
        ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Down, 0.30f, nullptr, &main_id);
    ImGuiID left_id = ImGui::DockBuilderSplitNode(main_id, ImGuiDir_Left, 0.26f, nullptr, &main_id);

    ImGui::DockBuilderDockWindow("Sector Overview", main_id);
    ImGui::DockBuilderDockWindow("Economy", left_id);
    ImGui::DockBuilderDockWindow("Command", bottom_id);
    ImGui::DockBuilderDockWindow("Systems", right_id);
    ImGui::DockBuilderDockWindow("Telemetry", right_id);
    ImGui::DockBuilderFinish(dockspace_id);
}

void Application::render_sector_overview_window()
{
    const game::ProductionSnapshot snapshot = game::forecast(game_state_);

    ImGui::Begin("Sector Overview");
    ImGui::TextUnformatted("Foundational idle-game loop scaffold");
    ImGui::TextDisabled(
        "SDL3 renderer path, Dear ImGui docking, JSON saves, and image loading are all live."
    );

    ImGui::SeparatorText("Resources");
    if (ImGui::BeginTable("resource_table", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        ImGui::Text("Power");
        ImGui::TableNextColumn();
        ImGui::Text("%s", format_amount(game_state_.power).c_str());

        ImGui::TableNextColumn();
        ImGui::Text("Ore");
        ImGui::TableNextColumn();
        ImGui::Text("%s", format_amount(game_state_.ore).c_str());

        ImGui::TableNextColumn();
        ImGui::Text("Alloys");
        ImGui::TableNextColumn();
        ImGui::Text("%s", format_amount(game_state_.alloys).c_str());

        ImGui::TableNextColumn();
        ImGui::Text("Science");
        ImGui::TableNextColumn();
        ImGui::Text("%s", format_amount(game_state_.science).c_str());

        ImGui::TableNextColumn();
        ImGui::Text("Credits");
        ImGui::TableNextColumn();
        ImGui::Text("%s", format_amount(game_state_.credits).c_str());

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Flow");
    ImGui::Text(
        "Net power: %+0.2f /s",
        snapshot.power_generation_per_second - snapshot.power_draw_per_second
    );
    ImGui::Text("Ore drift: %+0.2f /s", snapshot.ore_per_second);
    ImGui::Text("Alloys: %+0.2f /s", snapshot.alloys_per_second);
    ImGui::Text("Science: %+0.2f /s", snapshot.science_per_second);
    ImGui::Text("Credits: %+0.2f /s", snapshot.credits_per_second);

    ImGui::SeparatorText("Visual Pipeline");
    if (preview_texture_ != nullptr)
    {
        const float available_width = ImGui::GetContentRegionAvail().x;
        const float aspect_ratio = static_cast<float>(preview_texture_->height)
                                   / static_cast<float>(preview_texture_->width);
        ImGui::Image(
            reinterpret_cast<ImTextureID>(preview_texture_->texture),
            ImVec2(available_width, available_width * aspect_ratio)
        );
    }
    else
    {
        const std::string preview_path = preview_texture_path_.string();
        ImGui::TextWrapped(
            "No preview image found yet. Drop a file at %s to exercise the stb_image texture path.",
            preview_path.c_str()
        );
    }

    ImGui::End();
}

void Application::render_economy_window()
{
    ImGui::Begin("Economy");
    ImGui::TextUnformatted("Starter production hull");

    if (ImGui::BeginTable(
            "economy_table",
            5,
            ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg
                | ImGuiTableFlags_SizingStretchProp
        ))
    {
        ImGui::TableSetupColumn("Structure");
        ImGui::TableSetupColumn("Owned");
        ImGui::TableSetupColumn("Cost");
        ImGui::TableSetupColumn("Effect");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        const auto render_purchase_row = [this](game::StructureKind kind, const char* effect)
        {
            const game::StructureCost cost = game::cost_for(kind, game_state_);
            const bool affordable = can_afford(game_state_, cost);
            const std::string button_label = std::string("Build##") + game::label_for(kind);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(game::label_for(kind));
            ImGui::TableNextColumn();

            int owned = 0;
            switch (kind)
            {
                case game::StructureKind::SolarArray:
                    owned = game_state_.structures.solar_arrays;
                    break;
                case game::StructureKind::MiningDrone:
                    owned = game_state_.structures.mining_drones;
                    break;
                case game::StructureKind::Assembler:
                    owned = game_state_.structures.assemblers;
                    break;
                case game::StructureKind::Lab:
                    owned = game_state_.structures.labs;
                    break;
            }
            ImGui::Text("%d", owned);

            ImGui::TableNextColumn();
            ImGui::Text("%.0f cr / %.0f al", cost.credits, cost.alloys);

            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", effect);

            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!affordable);
            if (ImGui::SmallButton(button_label.c_str()) && game::try_purchase(game_state_, kind))
            {
                last_status_ = std::string("Constructed ") + game::label_for(kind) + ".";
            }
            ImGui::EndDisabled();
        };

        render_purchase_row(
            game::StructureKind::SolarArray, "+3.0 power generation and +15 storage"
        );
        render_purchase_row(game::StructureKind::MiningDrone, "+0.9 ore per second, draws power");
        render_purchase_row(game::StructureKind::Assembler, "Turns ore into alloys and credits");
        render_purchase_row(game::StructureKind::Lab, "Converts spare energy into science");

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Costs scale up from the current structure count.");
    ImGui::End();
}

void Application::render_systems_window()
{
    const game::ProductionSnapshot snapshot = game::forecast(game_state_);
    const float utilization = static_cast<float>(snapshot.power_utilization);

    ImGui::Begin("Systems");
    ImGui::Text("Simulation: %s", paused_ ? "paused" : "running");
    ImGui::ProgressBar(utilization, ImVec2(-1.0f, 0.0f), "Power utilization");

    ImGui::SeparatorText("Installations");
    ImGui::Text("Solar arrays: %d", game_state_.structures.solar_arrays);
    ImGui::Text("Mining drones: %d", game_state_.structures.mining_drones);
    ImGui::Text("Assemblers: %d", game_state_.structures.assemblers);
    ImGui::Text("Labs: %d", game_state_.structures.labs);

    ImGui::SeparatorText("Runtime");
    ImGui::Text("Power generation: %.2f /s", snapshot.power_generation_per_second);
    ImGui::Text("Power draw: %.2f /s", snapshot.power_draw_per_second);
    ImGui::Text("Session uptime: %.1f s", game_state_.session_seconds);
    ImGui::Text("Ticks processed: %llu", static_cast<unsigned long long>(game_state_.total_ticks));

    ImGui::Spacing();
    ImGui::TextWrapped(
        "This is intentionally small but structured: window lifecycle, simulation, save/load, and "
        "future art loading are already separated."
    );
    ImGui::End();
}

void Application::render_command_window()
{
    ImGui::Begin("Command");

    if (ImGui::Button(paused_ ? "Resume Simulation" : "Pause Simulation"))
    {
        paused_ = !paused_;
        last_status_ = paused_ ? "Simulation paused." : "Simulation resumed.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Manual Save"))
    {
        if (save_system_.save(game_state_))
        {
            last_status_ = "Saved sector state.";
        }
        else
        {
            last_status_ = save_system_.last_error();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Manual Load"))
    {
        if (const auto loaded = save_system_.load(); loaded.has_value())
        {
            game_state_ = *loaded;
            last_status_ = "Loaded sector state.";
        }
        else if (!save_system_.last_error().empty())
        {
            last_status_ = save_system_.last_error();
        }
        else
        {
            last_status_ = "No save file found yet.";
        }
    }

    if (ImGui::Button("Reset Sector"))
    {
        game_state_ = {};
        paused_ = false;
        last_status_ = "Sector reset to bootstrap defaults.";
    }

    ImGui::SeparatorText("Status");
    ImGui::TextWrapped("%s", last_status_.c_str());
    ImGui::TextWrapped(
        "This should give us a clean launch point for real progression systems, combat overlays, "
        "ship modules, and content data next."
    );

    ImGui::End();
}

void Application::render_telemetry_window()
{
    int window_width = 0;
    int window_height = 0;
    SDL_GetWindowSize(window_, &window_width, &window_height);

    int render_width = 0;
    int render_height = 0;
    SDL_GetCurrentRenderOutputSize(renderer_, &render_width, &render_height);

    ImGui::Begin("Telemetry");
    ImGui::Text("Renderer: %s", renderer_name_.c_str());
    ImGui::Text("Compiler: %s", __VERSION__);
    ImGui::Text("Frame time: %.2f ms", smoothed_frame_ms_);
    ImGui::Text("Window size: %d x %d", window_width, window_height);
    ImGui::Text("Render size: %d x %d", render_width, render_height);

    ImGui::SeparatorText("Persistence");
    const std::string save_path = save_system_.save_path().string();
    ImGui::TextWrapped("Save file: %s", save_path.c_str());

    ImGui::SeparatorText("Notes");
    ImGui::TextWrapped(
        "The current renderer path is SDL3's helper layer instead of raw OpenGL or Vulkan, "
        "matching the direction you wanted to test."
    );

    ImGui::End();
}

void Application::try_load_preview_texture()
{
    if (attempted_preview_load_)
    {
        return;
    }

    attempted_preview_load_ = true;
    std::string error;
    preview_texture_ = texture_cache_.load(renderer_, preview_texture_path_, &error);
    if (preview_texture_ != nullptr)
    {
        last_status_ = "Preview texture loaded through stb_image.";
    }
    else if (!error.empty())
    {
        last_status_ = error;
    }
}

}  // namespace star_idle
