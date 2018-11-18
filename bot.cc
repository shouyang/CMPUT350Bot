#include <sc2api/sc2_api.h>

#include <iostream>
#include <queue>
#include <algorithm>
#include <math.h>
#include <random>

#include "bot_examples.h"
#include "utils.h"

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

	// Note: ~1200 steps == 1 in-game minute
	size_t step_count = 0;

	int target_worker_count;
	double marine_to_maruader_ratio = 2.7; // Used By Barracks To Determine What To Produce


	// Queue of Enemy Sightings - Flushed Periodically
	std::queue<Point2D> enemy_unit_locations;

	// Constants Inherited
	// staging_location_ : Point2D location used for rallying created troops

public: // Public Functions Of Bot - On Event Handles Provided By Interface

	virtual void OnGameStart() final {
		// Call Setup Function of Multiplayer Bot -  Sets up Many Helpful constants
		MultiplayerBot::OnGameStart();
	}

	virtual void OnStep() final {
		step_count++;
		const ObservationInterface* observation = Observation();


		// Try To Avoid Doing Too Much Per Step Here
		// Using Prime Numbers Between 0-1200 (1 In-game minute) to offload some work..

		if (step_count % 3 == 0)
		{
			BuildOrder();
			ManageWorkers();
		}

		if (step_count % 103 == 0)
		{
			ManageRallyPoints();
			ManageDefense();
		}

		if (step_count % 367 == 0)
		{
			ManageIdleArmyUnits();
		}
		

		if (step_count % 891 == 0)
		{
			ManageUpgrades();
		}


		if (step_count % 1200 == 0)
		{
			ManageScouts();
			ManageAttack();
		}



		if (step_count % 2400 == 0)
		{
			FlushKnownEnemyLocations();
		}
	}

	virtual void OnUnitEnterVision(const sc2::Unit *unit)
	{
		// On sighting an enemy, record its position in the locations list.
		if (unit->alliance == Unit::Enemy)
		{
			enemy_unit_locations.push(unit->pos);
		}
	}

	virtual void OnBuildingConstructionComplete(const sc2::Unit* unit) {}

	virtual void OnUnitCreated(const sc2::Unit *unit)
	{
		// On Construction Of Combat Units, Rally Them To Staging Location.
		switch (unit->unit_type.ToType())
		{
		case UNIT_TYPEID::TERRAN_MARINE:
			GoToPoint(unit, staging_location_);
			break;
		case UNIT_TYPEID::TERRAN_MARAUDER:
			GoToPoint(unit, staging_location_);
			break;
		case UNIT_TYPEID::TERRAN_SIEGETANK:
			GoToPoint(unit, staging_location_);
			break;

		default:
			break;
		}
	}

	virtual void OnUnitDestroyed(const sc2::Unit *unit)
	{

	}

	virtual void OnUnitIdle(const Unit* unit) {
		// On Worker Idle, Assign Workers to Mineral Patch
		switch (unit->unit_type.ToType()) {
		case UNIT_TYPEID::TERRAN_SCV: {
			OnWorkerIdle(unit);
			break;
		}
		case UNIT_TYPEID::TERRAN_MULE: {
			OnWorkerIdle(unit);
			break;
		}
		// On Unit Production Idle, Delegate To Factory Handler
		case UNIT_TYPEID::TERRAN_BARRACKS: {
			HandleBarracks(unit);
			break;
		}
		case UNIT_TYPEID::TERRAN_FACTORY: {
			HandleFactory(unit);
			break;
		}
		// Note: Upgrade Building is handle in a manager function.

		default: {
			break;
		}
		}
	}


private: // Private Functions of Bot

	/*
	Manage Attack

	- Tries to attack move to a location where an enemy was sighted.
	- Tries only to attack past the six minute mark in steps and when there are a reasonable amount of troops.
	*/
	void ManageAttack()
	{
		const ObservationInterface* observation = Observation();
		// Setup - Get Army Units, new unit types must be added here for them to be included.
		Units marines = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		Units maruaders = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARAUDER));
		Units tanks = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

		bool past_six_minutes = step_count > 1200 * 6;
		bool significant_army = marines.size() + maruaders.size() > 20;

		// Check Attack Preconditions
		if (past_six_minutes && significant_army)
		{
			// Select Attack Location
			Point2D attack_location;

			if (enemy_unit_locations.size() > 0)
			{

				if (enemy_unit_locations.size() == 1)
				{
					attack_location = enemy_unit_locations.front();
				}
				else
				{
					size_t skip = GetRandomInteger(0, enemy_unit_locations.size() - 1);

					for (size_t i = 0; i < skip; i++)
					{
						enemy_unit_locations.pop();
					}
					attack_location = enemy_unit_locations.front();
				}


				// Order All Idle Units To Attack - new unit types must be added here for them to be included.
				for (const Unit* unit : marines)
				{
					if (unit->orders.empty() || Distance2D(unit->pos, staging_location_) < 15)
					{
						GoToPoint(unit, attack_location);
					}
				}

				for (const Unit* unit : maruaders)
				{
					if (unit->orders.empty() || Distance2D(unit->pos, staging_location_) < 15)
					{
						GoToPoint(unit, attack_location);
					}
				}

				for (const Unit* unit : tanks)
				{
					if (unit->orders.empty() || Distance2D(unit->pos, staging_location_) < 15)
					{
						GoToPoint(unit, attack_location);
					}
				}
			}
		}
	}


	/*
	Manage Workers

	Handles Economy Related Functions

	- Balances Workers To Minerals / Gas Refineries Via Call To Multiplayer Bot Function
	- Tries To Call Down MULE when able to.
	- Tries To Build To Expected (Ideal) Number Of Workers Plus 2 
	*/
	void ManageWorkers()
	{
		// Balance Workers Against Structures, ie CPs and Refineries 
		MultiplayerBot::ManageWorkers(UNIT_TYPEID::TERRAN_SCV, ABILITY_ID::HARVEST_GATHER, UNIT_TYPEID::TERRAN_REFINERY);

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

	/*
	Manage Upgrades
	
	Tries To Build Upgrades For Marines And Maruaders

	- Only Builds Upgrades When Five Minutes Have Past And There Exists A Reasonable Marine Maruader Force
	*/
	void ManageUpgrades()
	{
		const ObservationInterface* observation = Observation();
		// Setup - Get Upgrades Buildings and Unit Counts
		Units engineering_bays = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY));
		Units armories = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ARMORY));
		Units barracks_tech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB));
		Units factorys_tech = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORYTECHLAB));

		Units marines = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		Units maruaders = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARAUDER));


		bool past_five_minutes = step_count > 1200 * 5;
		size_t marine_maruader_count = marines.size() + maruaders.size();
		bool significant_bio_force = marine_maruader_count > 25;

		// Try To Build Upgrades If Army Is Sufficent And Game Has Progressed Far Enough
		if (past_five_minutes)
		{
			if (engineering_bays.size() > 0 && significant_bio_force)
			{
				const Unit* engineering_bay = engineering_bays.front();
				Actions()->UnitCommand(engineering_bay, ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONS);
				Actions()->UnitCommand(engineering_bay, ABILITY_ID::RESEARCH_TERRANINFANTRYARMOR);
			}

			if (barracks_tech.size() > 0 && significant_bio_force)
			{
				const Unit* barracks_tech_lab = barracks_tech.front();
				Actions()->UnitCommand(barracks_tech, ABILITY_ID::RESEARCH_COMBATSHIELD);
				Actions()->UnitCommand(barracks_tech, ABILITY_ID::RESEARCH_CONCUSSIVESHELLS);
			}
		}
	}

	/*
	Build Order

	Tries To Build Buildings Following Heuristics

	- Tries To Build Supply Depots When Near Supply Cap
	- Tries To Build Other Structures Via Heuristics
	- Tries To Expand Via Inherited Function
	*/
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
		Units armories = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ARMORY));
		Units orbitals = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_ORBITALCOMMAND));

		// Setup - Get Building Count Targets
		size_t barracks_count_target = std::min<size_t>(2 * bases.size(), 8);
		size_t factory_count_target = 1;
		size_t engineering_bay_count_target = 1;
		size_t armory_count_target = 1;

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

		if (observation->GetFoodUsed() >= FoodCapInProgress - 3 && observation->GetFoodCap() != 200)
		{
			TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);

			if (observation->GetMinerals() > 250 && observation->GetFoodUsed() == observation->GetFoodCap())
			{
				TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
			}
		}

		// Try Build Refinery - Do not over build refineries, keep pace with orbital command centers
		if (barracks.size() >= 2 && orbitals.size() >= 1 && refinerys.size() < orbitals.size())
		{
			for (auto u : bases)
			{
				TryBuildGas(ABILITY_ID::BUILD_REFINERY, UNIT_TYPEID::TERRAN_SCV, Point2D(u->pos.x, u->pos.y));
			}
		}

		// Try Build Barracks - Try To Build Barracks Depending On Number Of Bases
		if (barracks.size() < barracks_count_target && observation->GetMinerals() > 170)
		{
			TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
		}

		// Try Build Factory - Try To Build Factories Only After Having Sufficent Barracks
		if (factorys.size() < factory_count_target && barracks.size() > 3)
		{
			TryBuildStructure(ABILITY_ID::BUILD_FACTORY);
		}

		// Try Build Engineering Bay
		if (engineering_bays.size() < engineering_bay_count_target && barracks.size() > 3)
		{
			TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
		}

		// Try Build Armory
		if (armories.size() < armory_count_target && bases.size() > 2 && factorys.size() > 0 && engineering_bays.size() > 0)
		{
			TryBuildStructure(ABILITY_ID::BUILD_ARMORY);
		}

		// Try Expand
		if (barracks.size() >= 2 && bases.size() <= 3 && observation->GetMinerals() > 400 * bases.size())
		{
			TryExpand(ABILITY_ID::BUILD_COMMANDCENTER, UNIT_TYPEID::TERRAN_SCV);
		}


		// Try Build Barracks Addons
		// Moved To Barracks On Idle

		// Try Build Factory Addons
		// Moved To Factory On Idle
	}

	/*
	Manage Rally Points

	Manages The Staging Point Location Used To Rally Newly Created Troops

	- Tries to place the rally point to the closest expansion to the middle of the map
	- Shifts units slightly such that they are in front of the base, closer to the map center.
	*/
	void ManageRallyPoints()
	{
		const ObservationInterface* observation = Observation();
		Units bases = observation->GetUnits(Unit::Self, IsTownHall());

		if (bases.size() == 1)
		{
			return;
		}

		if (bases.size() > 1)
		{
			const Unit* base_closest_to_mid = bases.front();

			for (const Unit *u : bases)
			{
				if (Distance2D(u->pos, getMapCenter(observation)) < (Distance2D(base_closest_to_mid->pos, getMapCenter(observation))))
				{
					base_closest_to_mid = u;
				}
			}


			Point2D unit_vector_to_map_center = pointTowards(base_closest_to_mid->pos, getMapCenter(Observation()));

			staging_location_.x = base_closest_to_mid->pos.x + unit_vector_to_map_center.x * 5.0f;
			staging_location_.y = base_closest_to_mid->pos.y + unit_vector_to_map_center.y * 4.0f;
		}

	}

	/*
	Manage Scouts

	Sends Scouts To Scout The Map

	- Tries to scout potentical enemy spawns if the game is relatively early.
	- Tries to scout random pathable locations if the game has gone on for sometime.
	- If a game has gone on for some time, we likely have constant contact with the enemy already
	or their original base may have been destroyed. Hence we use randomness here to help uncover more area.

	*/
	void ManageScouts()
	{
		const ObservationInterface* observation = Observation();

		bool in_first_15_minutes = step_count < 1200 * 15;

		if (in_first_15_minutes)
		{
			for (Point2D point : game_info_.enemy_start_locations)
			{
				const Unit* unit;
				GetRandomUnit(unit, observation, UNIT_TYPEID::TERRAN_MARINE);

				if (unit && unit->orders.empty())
				{
					Actions()->UnitCommand(unit, ABILITY_ID::SMART, point);
				}
			}
		}
		else
		{
			for (int i = 0; i < 3; i++)
			{
				const Unit* unit;
				GetRandomUnit(unit, observation, UNIT_TYPEID::TERRAN_MARINE);

				if (unit && unit->orders.empty())
				{
					ScoutWithUnit(unit, observation);
				}

			}
		}


	}

	/*
	Mange Idle Army Units

	Rallies Army Units To The Staging Location If They Have No Other Orders

	*/
	void ManageIdleArmyUnits()
	{
		const ObservationInterface* observation = Observation();

		Units marines = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		Units maruaders = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARAUDER));
		Units tanks = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

		for (const Unit* unit : marines)
		{
			if (unit->orders.empty())
			{
				GoToPoint(unit, staging_location_);
			}
		}

		for (const Unit* unit : maruaders)
		{
			if (unit->orders.empty())
			{
				GoToPoint(unit, staging_location_);
			}
		}

		for (const Unit* unit : tanks)
		{
			if (unit->orders.empty())
			{
				GoToPoint(unit, staging_location_);
			}
		}
	}

	/*
	Manage Defense

	Uses Idle Units To Defend Bases And Unit Staging Lcoation

	- Checks Each Enemy Unit Location, if close to base or staging attacks using Idle Units.

	*/
	void ManageDefense()
	{
		const ObservationInterface* observation = Observation();

		Units bases = observation->GetUnits(Unit::Self, IsTownHall());

		Units marines = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
		Units maruaders = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_MARAUDER));
		Units tanks = observation->GetUnits(Unit::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));

		Units enemy_units = observation->GetUnits(Unit::Enemy);

		for (const Unit* enemy_unit : enemy_units)
		{
			bool close_to_staging_point = Distance2D(enemy_unit->pos, staging_location_) < 20.0f;
			bool close_to_base = false;

			for (const Unit* base_unit : bases)
			{
				if (Distance2D(enemy_unit->pos, base_unit->pos) < 20.0f)
				{
					close_to_base = true;
				}
			}

			if (close_to_staging_point || close_to_base)
			{
				for (const Unit* army_unit : marines)
				{
					if (army_unit->orders.empty())
					{
						GoToPoint(army_unit, enemy_unit->pos);
					}
				}

				for (const Unit* army_unit : maruaders)
				{
					if (army_unit->orders.empty())
					{
						GoToPoint(army_unit, enemy_unit->pos);
					}
				}

				for (const Unit* army_unit : tanks)
				{
					if (army_unit->orders.empty())
					{
						GoToPoint(army_unit, enemy_unit->pos);
					}
				}
			}
		}
	}
	// Per Factory Functions
	void HandleBarracks(const Unit* unit)
	{
		size_t tech_lab_count = CountUnitType(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB) + 1;
		size_t reactor_count  = CountUnitType(UNIT_TYPEID::TERRAN_BARRACKSREACTOR) + 1;

		size_t marine_count   = CountUnitType(UNIT_TYPEID::TERRAN_MARINE) + 1;
		size_t maruader_count = CountUnitType(UNIT_TYPEID::TERRAN_MARAUDER) + 1;


		if ((tech_lab_count / reactor_count) < 2)
		{
			TryBuildAddOn(ABILITY_ID::BUILD_TECHLAB_BARRACKS, unit->tag);
		}
		else
		{
			TryBuildAddOn(ABILITY_ID::BUILD_REACTOR_BARRACKS, unit->tag);
		}

		if (marine_count / maruader_count > marine_to_maruader_ratio)
		{
			Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARAUDER);
		}

		if (unit->orders.empty())
		{
			Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
		}

	}

	void HandleFactory(const Unit* unit)
	{
		TryBuildAddOn(ABILITY_ID::BUILD_TECHLAB_FACTORY, unit->tag);
		Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SIEGETANK);
	}


	// Per Unit Functions

	void GoToPoint(const Unit* unit, Point2D point)
	{
		Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, point);
	}

	void OnWorkerIdle(const Unit* unit)
	{
		const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
		if (!mineral_target)
			return;

		Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
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

		if (unit_to_build)
		{
			Actions()->UnitCommand(unit_to_build,
				ability_type_for_structure,
				Point2D(unit_to_build->pos.x + rx * 10.0f, unit_to_build->pos.y + ry * 10.0f));
		}


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

	void FlushKnownEnemyLocations()
	{
		size_t number_of_positions_to_flush = floor(enemy_unit_locations.size() * 0.9);

		for (size_t i = 0; i < number_of_positions_to_flush; i++)
		{
			if (!enemy_unit_locations.empty())
			{
				enemy_unit_locations.pop();
			}
		}
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
		CreateComputer(Race::Protoss, Difficulty::Hard)
		});

	coordinator.LaunchStarcraft();
	coordinator.StartGame("Ladder/CactusValleyLE.SC2Map");

	while (coordinator.Update()) {
	}

	return 0;
}