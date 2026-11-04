#pragma once
// Game definitions the operational layer needs, compiled in from data/ by
// scripts/gen-sim-data.mjs (see sim/generated/HavocData.inc). No JSON parsing
// at runtime and no floating point: every quantity is a Fixed64.
#include <cstdint>
#include <string>
#include <vector>

#include "havoc/Fixed.h"

namespace havoc::ops {

struct UnitDef {
  std::string id;
  std::string faction;
  std::int32_t crew = 0;
  Fixed64 fuelCapacity;     // kilolitres
  Fixed64 fuelBurnPerDay;   // kilolitres per game-day of operation
  Fixed64 ammoCrates;       // basic ammunition load, crates
  Fixed64 maintenancePerDay;  // repair-hours per game-day of operation
};

struct FormationEntry {
  std::size_t unit = 0;  // index into DataSet::units
  std::int32_t count = 0;
};

struct FormationDef {
  std::string id;
  std::string faction;
  std::vector<FormationEntry> entries;
};

struct DataSet {
  std::vector<UnitDef> units;            // sorted by id
  std::vector<FormationDef> formations;  // sorted by id
  Fixed64 foodPerCrewPerDay;     // crates
  Fixed64 medicalPerCrewPerDay;  // crates

  // Returns nullptr when the id is unknown.
  const UnitDef* FindUnit(const std::string& id) const;
  const FormationDef* FindFormation(const std::string& id) const;
};

// The data set built from data/ at generation time.
const DataSet& BuiltinData();

}  // namespace havoc::ops
