#include <sc2api/sc2_api.h>
#include "bot_examples.h"

#include <iostream>
#include <queue>

#include <vector>
#include <unordered_map>

using namespace sc2;


// These Two Structs Were Needed From "bot_examples.cc"
struct IsTownHall {
    bool operator()(const Unit& unit) {
        switch (unit.unit_type.ToType()) {
        case UNIT_TYPEID::ZERG_HATCHERY: return true;
        case UNIT_TYPEID::ZERG_LAIR: return true;
        case UNIT_TYPEID::ZERG_HIVE: return true;
        case UNIT_TYPEID::TERRAN_COMMANDCENTER: return true;
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND: return true;
        case UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING: return true;
        case UNIT_TYPEID::TERRAN_PLANETARYFORTRESS: return true;
        case UNIT_TYPEID::PROTOSS_NEXUS: return true;
        default: return false;
        }
    }
};

struct IsStructure {
    IsStructure(const ObservationInterface* obs) : observation_(obs) {};

    bool operator()(const Unit& unit) {
        auto& attributes = observation_->GetUnitTypeData().at(unit.unit_type).attributes;
        bool is_structure = false;
        for (const auto& attribute : attributes) {
            if (attribute == Attribute::Structure) {
                is_structure = true;
            }
        }
        return is_structure;
    }

    const ObservationInterface* observation_;
};


class Bot : public MultiplayerBot {

private: // Bot Global Variables
    size_t step_count = 0;

    Point2D spawn;
    Point2D enemy_spawn; // Find via scouting ( Observation()->GetGameInfo().enemy_start_locations returns a std::vector<Point2D> )

    int target_worker_count;

    // Defines our target wave composition
    std::unordered_map<UNIT_TYPEID, int> WAVECOMP = {
        { UNIT_TYPEID::TERRAN_MARINE, 10 }
    };

    // Holds vectors of unit pointers. Each vector represents one wave
    std::vector<std::vector <const Unit*>> waves;

    // Temp
    // Tag identifying our chosen scouting SCV
    Tag scout = 0;

public:
    virtual void OnGameStart() final {
        MultiplayerBot::OnGameStart();

        // Game etiquette
        //Actions()->SendChat("glhf");

        // Set spawn
        const ObservationInterface* observation = Observation();
        Point3D temp = observation->GetStartLocation();
        spawn = Point2D(temp.x, temp.y);

        // waves must be initialized with an empty wave
        std::vector<const Unit*> w;
        waves.push_back(w);
    }

    virtual void OnStep() final {
        step_count++;

        handleWaves();

        BuildOrder();

        ManageWorkers();
    }

    virtual void OnUnitEnterVision(const sc2::Unit *unit)
    {

    }

    virtual void OnBuildingConstructionComplete(const sc2::Unit* unit)
    {

    }

    virtual void OnUnitCreated(const sc2::Unit *unit)
    {
        switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_SCV: {
            if (scout == 0) {
                scout = unit->tag;
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_MARINE: {
            addToWave(unit);
            break;
        }
        }
    }

    virtual void OnUnitDestroyed(const sc2::Unit *unit)
    {

    }

    virtual void OnUnitIdle(const Unit* unit) {

        switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_SCV: {

            if (unit->tag == scout) {
                ScoutWithUnit(unit, Observation());
                break;
            }

            const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
            if (!mineral_target) {
                break;
            }
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            break;
        }
        case UNIT_TYPEID::TERRAN_MULE: {
            const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
            if (!mineral_target) {
                break;
            }
            Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            break;
        }
        case UNIT_TYPEID::TERRAN_BARRACKS: {

            // TODO: Add Actual Logic on Deciding Which Addon To Build
            TryBuildAddOn(ABILITY_ID::BUILD_TECHLAB_BARRACKS, unit->tag);

            // TODO: Add Actual Logic on Deciding Which Unit To Build
            Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);

            break;
        }
        case UNIT_TYPEID::TERRAN_MARINE: {
            const GameInfo& game_info = Observation()->GetGameInfo();

            Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, Point2D(staging_location_.x, staging_location_.y));

            break;
        }
        default: {
            break;
        }
        }
    }


private:
    // Returns the number of the given unit type in the given wave
    int countTypeInWave(UNIT_TYPEID uid, std::vector<const Unit*> wave) {
        int count = 0;
        for (int i = 0; i < wave.size(); ++i) {
            if (wave[i]->unit_type.ToType() == uid) {
                ++count;
            }
        }
        return count;
    }

    void addToWave(const Unit* u) {
        if (countTypeInWave(u->unit_type.ToType(), waves[waves.size() - 1]) < WAVECOMP[u->unit_type.ToType()]) {
            // Can fit more of this unit type in this wave
            waves[waves.size() - 1].push_back(u);
        }
        else {
            // No empty spots found, create new wave and add.
            std::vector<const Unit*> w;
            waves.push_back(w);
            waves[waves.size() - 1].push_back(u);
        }
        //std::cout << "Added " << UnitTypeToName(u->unit_type.ToType()) << " to wave " << waves.size() - 1 << std::endl;
    }

    // Returns true iff the given wave has a complete composition
    bool waveReady(std::vector<const Unit*> wave) {
        for (std::pair<UNIT_TYPEID, int> type : WAVECOMP) {
            if (countTypeInWave(type.first, wave) < type.second) {
                return false;
            }
        }
        return true;
    }

    // If any given wave has a complete composition, order it to attack
    void handleWaves() {
        const ObservationInterface* observation = Observation();
        for (auto wave : waves) {
            if (waveReady(wave)) {
                for (auto unit : wave) {
                    // For now this just picks a random possible enemy location, later we'll scout for the actual spawn.
                    Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, Observation()->GetGameInfo().enemy_start_locations[0]);
                }
                // Can we remove the wave after issuing this command?
            }
        }
    }

    size_t CountUnitType(UNIT_TYPEID unit_type) {
        return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
    }

    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
        const ObservationInterface* observation = Observation();

        // If a unit already is building a supply structure of this type, do nothing.
        // Also get an scv to build the structure.
        const Unit* unit_to_build = nullptr;
        Units units = observation->GetUnits(Unit::Alliance::Self);
        for (const auto& unit : units) {
            for (const auto& order : unit->orders) {
                if (order.ability_id == ability_type_for_structure) {
                    continue;
                }
            }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
            }
        }

        float rx = GetRandomScalar();
        float ry = GetRandomScalar();

        Actions()->UnitCommand(unit_to_build,
            ability_type_for_structure,
            Point2D(unit_to_build->pos.x + rx * 10.0f, unit_to_build->pos.y + ry * 10.0f));

        return true;
    }

    bool TryBuildAddOn(AbilityID ability_type_for_structure, Tag base_structure) {
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        const Unit* unit = Observation()->GetUnit(base_structure);

        if (unit->build_progress != 1) {
            return false;
        }

        Point2D build_location = Point2D(unit->pos.x + rx * 15, unit->pos.y + ry * 15);

        Units units = Observation()->GetUnits(Unit::Self, IsStructure(Observation()));

        if (Query()->Placement(ability_type_for_structure, unit->pos, unit)) {
            Actions()->UnitCommand(unit, ability_type_for_structure);
            return true;
        }

        float distance = std::numeric_limits<float>::max();
        for (const auto& u : units) {
            float d = Distance2D(u->pos, build_location);
            if (d < distance) {
                distance = d;
            }
        }
        if (distance < 6) {
            return false;
        }

        if (Query()->Placement(ability_type_for_structure, build_location, unit)) {
            Actions()->UnitCommand(unit, ability_type_for_structure, build_location);
            return true;
        }
        return false;

    }

    void ManageWorkers()
    {
        MultiplayerBot::ManageWorkers(UNIT_TYPEID::TERRAN_SCV, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::TERRAN_REFINERY);
        // Above: Balance Workers Against Structures, ie CPs and Refineries 

        // Try To Use The Mule Call Down Whenever Possible On Orbital Command Centers
        const ObservationInterface* observation = Observation();
        Units orbitals = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ORBITALCOMMAND));
        for (auto &unit : orbitals)
        {
            if (unit->energy > 50)
            {
                float rx = GetRandomScalar();
                float ry = GetRandomScalar();

                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_CALLDOWNMULE, Point2D(unit->pos.x + rx * 2, unit->pos.y + ry * 2));
            }
        }

        // Try to Build Up To The Optimal Number of Workers Plus 2 For Construction
        target_worker_count = GetExpectedWorkers(UNIT_TYPEID::TERRAN_REFINERY) + 2;
        if (CountUnitType(UNIT_TYPEID::TERRAN_SCV) < target_worker_count) {
            TryBuildUnit(ABILITY_ID::TRAIN_SCV, UNIT_TYPEID::TERRAN_COMMANDCENTER);
            TryBuildUnit(ABILITY_ID::TRAIN_SCV, UNIT_TYPEID::TERRAN_ORBITALCOMMAND);
        }


    }

    void BuildOrder() {
        const ObservationInterface* observation = Observation();
        // Setup - Get Building Counts
        Units bases = observation->GetUnits(Unit::Self, IsTownHall());
        Units barracks = observation->GetUnits(Unit::Self, IsUnits(barrack_types));
        Units factorys = observation->GetUnits(Unit::Self, IsUnits(factory_types));
        Units starports = observation->GetUnits(Unit::Self, IsUnits(starport_types));
        Units barracks_tech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
        Units factorys_tech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYTECHLAB));
        Units starports_tech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORTTECHLAB));
        Units supply_depots = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));
        Units refinerys = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
        Units engineering_bays = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
        Units orbitals = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ORBITALCOMMAND));

        // Setup - Get Building Count Targets
        size_t barracks_count_target = std::min<size_t>(2 * bases.size(), 8);
        size_t factory_count_target = 1;
        size_t engineering_bay_count_target = 1;


        // Build


        // Try Convert Command Center
        if (!barracks.empty())
        {
            for (const auto& base : bases) {
                if (base->unit_type == UNIT_TYPEID::TERRAN_COMMANDCENTER && observation->GetMinerals() > 150) {
                    Actions()->UnitCommand(base, ABILITY_ID::MORPH_ORBITALCOMMAND);
                }
            }
        }

        // Try Build Depot - Calculate our own total supply cap here to account for buildings in progress..
        size_t FoodCapInProgress = supply_depots.size() * 8 + bases.size() * 15;

        if (observation->GetFoodUsed() >= FoodCapInProgress - 3)
        {
            TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
        }

        // Try Build Refinery
        if (barracks.size() >= 2 && orbitals.size() >= 1 && refinerys.size() < orbitals.size())
        {
            for (auto u : bases)
            {
                TryBuildGas(ABILITY_ID::BUILD_REFINERY, UNIT_TYPEID::TERRAN_SCV, Point2D(u->pos.x, u->pos.y));
            }
        }

        // Try Build Barracks
        if (barracks.size() < barracks_count_target && observation->GetMinerals() > 170)
        {
            TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
        }

        // Try Build Factory
        if (factorys.size() < factory_count_target && barracks.size() > 3)
        {
            TryBuildStructure(ABILITY_ID::BUILD_FACTORY);
        }

        // Try Build Engineering Bay
        if (engineering_bays.size() < engineering_bay_count_target && barracks.size() > 3)
        {
            TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
        }

        // Try Expand
        if (barracks.size() >= 2 && bases.size() <= 2 && observation->GetMinerals() > 400 * bases.size())
        {
            TryExpand(ABILITY_ID::BUILD_COMMANDCENTER, UNIT_TYPEID::TERRAN_SCV);
        }


        // Try Build Barracks Addons
        // TODO - May be moved to Barracks IDLE

        // Try Build Factory Addons
        // TODO - May be moved to Barracks IDLE
    }

    // Helper Functions
    const Unit* FindNearestMineralPatch(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (const auto& u : units) {
            if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
                float d = DistanceSquared2D(u->pos, start);
                if (d < distance) {
                    distance = d;
                    target = u;
                }
            }
        }
        return target;
    }

    // Constant Types From Example Terran Bot
    std::vector<UNIT_TYPEID> barrack_types = { UNIT_TYPEID::TERRAN_BARRACKSFLYING, UNIT_TYPEID::TERRAN_BARRACKS };
    std::vector<UNIT_TYPEID> factory_types = { UNIT_TYPEID::TERRAN_FACTORYFLYING, UNIT_TYPEID::TERRAN_FACTORY };
    std::vector<UNIT_TYPEID> starport_types = { UNIT_TYPEID::TERRAN_STARPORTFLYING, UNIT_TYPEID::TERRAN_STARPORT };
    std::vector<UNIT_TYPEID> supply_depot_types = { UNIT_TYPEID::TERRAN_SUPPLYDEPOT, UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED };
    std::vector<UNIT_TYPEID> bio_types = { UNIT_TYPEID::TERRAN_MARINE, UNIT_TYPEID::TERRAN_MARAUDER, UNIT_TYPEID::TERRAN_GHOST, UNIT_TYPEID::TERRAN_REAPER /*reaper*/ };
    std::vector<UNIT_TYPEID> widow_mine_types = { UNIT_TYPEID::TERRAN_WIDOWMINE, UNIT_TYPEID::TERRAN_WIDOWMINEBURROWED };
    std::vector<UNIT_TYPEID> siege_tank_types = { UNIT_TYPEID::TERRAN_SIEGETANK, UNIT_TYPEID::TERRAN_SIEGETANKSIEGED };
    std::vector<UNIT_TYPEID> viking_types = { UNIT_TYPEID::TERRAN_VIKINGASSAULT, UNIT_TYPEID::TERRAN_VIKINGFIGHTER };
    std::vector<UNIT_TYPEID> hellion_types = { UNIT_TYPEID::TERRAN_HELLION, UNIT_TYPEID::TERRAN_HELLIONTANK };
};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

    Bot bot;
    coordinator.SetParticipants({
        CreateParticipant(Race::Terran, &bot),
        CreateComputer(Race::Zerg)
    });

    coordinator.LaunchStarcraft();
    coordinator.StartGame("Ladder/CactusValleyLE.SC2Map");

    while (coordinator.Update()) {
    }

    return 0;
}