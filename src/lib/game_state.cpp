#include "game_state.hpp"

#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace star_idle::game
{

namespace
{

constexpr f64 kPowerPerSolarArray = 3.0;
constexpr f64 kPowerStoragePerSolarArray = 15.0;
constexpr f64 kBasePowerStorage = 120.0;
constexpr f64 kPowerDrawPerMiner = 0.65;
constexpr f64 kPowerDrawPerAssembler = 1.10;
constexpr f64 kPowerDrawPerLab = 0.90;
constexpr f64 kOrePerMiner = 0.90;
constexpr f64 kAlloysPerAssembler = 0.35;
constexpr f64 kOrePerAlloy = 2.0;
constexpr f64 kSciencePerLab = 0.18;
constexpr f64 kCreditsPerAlloy = 5.5;

[[nodiscard]] f64 storage_capacity(const GameState& state)
{
    return kBasePowerStorage
           + (static_cast<double>(state.structures.solar_arrays) * kPowerStoragePerSolarArray);
}

[[nodiscard]] f64 rounded_cost(f64 value)
{
    return std::round(value);
}

[[nodiscard]] f64 scaled_cost(int owned, f64 base_cost, f64 growth)
{
    return rounded_cost(base_cost * std::pow(growth, static_cast<double>(owned)));
}

[[nodiscard]] int owned_count(StructureKind kind, const GameState& state)
{
    switch (kind)
    {
        case StructureKind::SolarArray:
            return state.structures.solar_arrays;
        case StructureKind::MiningDrone:
            return state.structures.mining_drones;
        case StructureKind::Assembler:
            return state.structures.assemblers;
        case StructureKind::Lab:
            return state.structures.labs;
    }

    return 0;
}

}  // namespace

ProductionSnapshot forecast(const GameState& state)
{
    ProductionSnapshot snapshot{};
    snapshot.power_generation_per_second =
        static_cast<double>(state.structures.solar_arrays) * kPowerPerSolarArray;
    snapshot.power_draw_per_second =
        (static_cast<double>(state.structures.mining_drones) * kPowerDrawPerMiner)
        + (static_cast<double>(state.structures.assemblers) * kPowerDrawPerAssembler)
        + (static_cast<double>(state.structures.labs) * kPowerDrawPerLab);

    if (snapshot.power_draw_per_second > 0.0)
    {
        if (state.power > 0.0
            || snapshot.power_generation_per_second >= snapshot.power_draw_per_second)
        {
            snapshot.power_utilization = 1.0;
        }
        else
        {
            snapshot.power_utilization = std::clamp(
                snapshot.power_generation_per_second / snapshot.power_draw_per_second, 0.0, 1.0
            );
        }
    }

    const double ore_mined = static_cast<double>(state.structures.mining_drones) * kOrePerMiner
                             * snapshot.power_utilization;
    const double requested_alloys = static_cast<double>(state.structures.assemblers)
                                    * kAlloysPerAssembler * snapshot.power_utilization;
    const double max_alloys_from_ore = (state.ore + ore_mined) / kOrePerAlloy;
    snapshot.alloys_per_second = std::max(0.0, std::min(requested_alloys, max_alloys_from_ore));
    snapshot.ore_per_second = ore_mined - (snapshot.alloys_per_second * kOrePerAlloy);
    snapshot.science_per_second =
        static_cast<double>(state.structures.labs) * kSciencePerLab * snapshot.power_utilization;
    snapshot.credits_per_second = snapshot.alloys_per_second * kCreditsPerAlloy;

    return snapshot;
}

void advance(GameState& state, f64 delta_seconds)
{
    const ProductionSnapshot snapshot = forecast(state);

    state.power = std::clamp(
        state.power
            + ((snapshot.power_generation_per_second - snapshot.power_draw_per_second)
               * delta_seconds),
        0.0,
        storage_capacity(state)
    );

    state.ore += static_cast<double>(state.structures.mining_drones) * kOrePerMiner
                 * snapshot.power_utilization * delta_seconds;

    const double desired_alloys = static_cast<double>(state.structures.assemblers)
                                  * kAlloysPerAssembler * snapshot.power_utilization
                                  * delta_seconds;
    const double max_alloys_from_ore = state.ore / kOrePerAlloy;
    const double produced_alloys = std::max(0.0, std::min(desired_alloys, max_alloys_from_ore));
    state.ore -= produced_alloys * kOrePerAlloy;
    state.alloys += produced_alloys;
    state.credits += produced_alloys * kCreditsPerAlloy;
    state.science += static_cast<double>(state.structures.labs) * kSciencePerLab
                     * snapshot.power_utilization * delta_seconds;
    state.session_seconds += delta_seconds;
    ++state.total_ticks;
}

StructureCost cost_for(StructureKind kind, const GameState& state)
{
    const int owned = owned_count(kind, state);

    switch (kind)
    {
        case StructureKind::SolarArray:
            return StructureCost{
                .credits = scaled_cost(owned, 24.0, 1.18),
                .alloys = scaled_cost(owned, 4.0, 1.10),
            };
        case StructureKind::MiningDrone:
            return StructureCost{
                .credits = scaled_cost(owned, 36.0, 1.21),
                .alloys = scaled_cost(owned, 8.0, 1.14),
            };
        case StructureKind::Assembler:
            return StructureCost{
                .credits = scaled_cost(owned, 58.0, 1.24),
                .alloys = scaled_cost(owned, 14.0, 1.16),
            };
        case StructureKind::Lab:
            return StructureCost{
                .credits = scaled_cost(owned, 82.0, 1.28),
                .alloys = scaled_cost(owned, 20.0, 1.20),
            };
    }

    return {};
}

bool try_purchase(GameState& state, StructureKind kind)
{
    const StructureCost cost = cost_for(kind, state);
    if (state.credits < cost.credits || state.alloys < cost.alloys)
    {
        return false;
    }

    state.credits -= cost.credits;
    state.alloys -= cost.alloys;

    switch (kind)
    {
        case StructureKind::SolarArray:
            ++state.structures.solar_arrays;
            break;
        case StructureKind::MiningDrone:
            ++state.structures.mining_drones;
            break;
        case StructureKind::Assembler:
            ++state.structures.assemblers;
            break;
        case StructureKind::Lab:
            ++state.structures.labs;
            break;
    }

    return true;
}

const char* label_for(StructureKind kind)
{
    switch (kind)
    {
        case StructureKind::SolarArray:
            return "Solar Array";
        case StructureKind::MiningDrone:
            return "Mining Drone";
        case StructureKind::Assembler:
            return "Assembler";
        case StructureKind::Lab:
            return "Lab";
    }

    return "Unknown";
}

void to_json(nlohmann::json& json, const GameState& state)
{
    json = nlohmann::json{
        {"resources",
         {
             {"power", state.power},
             {"ore", state.ore},
             {"alloys", state.alloys},
             {"science", state.science},
             {"credits", state.credits},
         }},
        {"structures",
         {
             {"solar_arrays", state.structures.solar_arrays},
             {"mining_drones", state.structures.mining_drones},
             {"assemblers", state.structures.assemblers},
             {"labs", state.structures.labs},
         }},
        {"meta",
         {
             {"session_seconds", state.session_seconds},
             {"total_ticks", state.total_ticks},
         }},
    };
}

void from_json(const nlohmann::json& json, GameState& state)
{
    const nlohmann::json resources = json.value("resources", nlohmann::json::object());
    const nlohmann::json structures = json.value("structures", nlohmann::json::object());
    const nlohmann::json meta = json.value("meta", nlohmann::json::object());

    state.power = resources.value("power", state.power);
    state.ore = resources.value("ore", state.ore);
    state.alloys = resources.value("alloys", state.alloys);
    state.science = resources.value("science", state.science);
    state.credits = resources.value("credits", state.credits);

    state.structures.solar_arrays = structures.value("solar_arrays", state.structures.solar_arrays);
    state.structures.mining_drones =
        structures.value("mining_drones", state.structures.mining_drones);
    state.structures.assemblers = structures.value("assemblers", state.structures.assemblers);
    state.structures.labs = structures.value("labs", state.structures.labs);

    state.session_seconds = meta.value("session_seconds", state.session_seconds);
    state.total_ticks = meta.value("total_ticks", state.total_ticks);
}

}  // namespace star_idle::game
