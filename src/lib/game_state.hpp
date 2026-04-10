#pragma once

#include "common.hpp"

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
    f64 power{40.0};
    f64 ore{12.0};
    f64 alloys{4.0};
    f64 science{0.0};
    f64 credits{90.0};
    f64 session_seconds{0.0};
    u64 total_ticks{0};
    StructureCounts structures{};
};

struct ProductionSnapshot
{
    f64 power_generation_per_second{0.0};
    f64 power_draw_per_second{0.0};
    f64 power_utilization{1.0};
    f64 ore_per_second{0.0};
    f64 alloys_per_second{0.0};
    f64 science_per_second{0.0};
    f64 credits_per_second{0.0};
};

struct StructureCost
{
    f64 credits{0.0};
    f64 alloys{0.0};
};

[[nodiscard]] ProductionSnapshot forecast(const GameState& state);
void advance(GameState& state, f64 delta_seconds);
[[nodiscard]] StructureCost cost_for(StructureKind kind, const GameState& state);
[[nodiscard]] bool try_purchase(GameState& state, StructureKind kind);
[[nodiscard]] const char* label_for(StructureKind kind);

void to_json(nlohmann::json& json, const GameState& state);
void from_json(const nlohmann::json& json, GameState& state);

}  // namespace star_idle::game
