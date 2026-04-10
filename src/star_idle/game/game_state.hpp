#pragma once

#include <cstdint>
#include <nlohmann/json_fwd.hpp>

namespace star_idle::game
{

enum class StructureKind
{
    SolarArray,
    MiningDrone,
    Assembler,
    Lab,
};

struct StructureCounts
{
    int solar_arrays{2};
    int mining_drones{1};
    int assemblers{1};
    int labs{0};
};

struct GameState
{
    double power{40.0};
    double ore{12.0};
    double alloys{4.0};
    double science{0.0};
    double credits{90.0};
    double session_seconds{0.0};
    std::uint64_t total_ticks{0};
    StructureCounts structures{};
};

struct ProductionSnapshot
{
    double power_generation_per_second{0.0};
    double power_draw_per_second{0.0};
    double power_utilization{1.0};
    double ore_per_second{0.0};
    double alloys_per_second{0.0};
    double science_per_second{0.0};
    double credits_per_second{0.0};
};

struct StructureCost
{
    double credits{0.0};
    double alloys{0.0};
};

[[nodiscard]] ProductionSnapshot forecast(const GameState& state);
void advance(GameState& state, double delta_seconds);
[[nodiscard]] StructureCost cost_for(StructureKind kind, const GameState& state);
[[nodiscard]] bool try_purchase(GameState& state, StructureKind kind);
[[nodiscard]] const char* label_for(StructureKind kind);

void to_json(nlohmann::json& json, const GameState& state);
void from_json(const nlohmann::json& json, GameState& state);

}  // namespace star_idle::game
