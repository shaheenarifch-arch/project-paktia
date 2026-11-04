#include "havoc/ops/Data.h"

#include <algorithm>

namespace havoc::ops {
namespace {

Fixed64 Milli(std::int64_t v) { return Fixed64::FromRatio(v, 1000); }

DataSet Build() {
  DataSet d;
  std::int64_t foodMilli = 0, medicalMilli = 0;
#define HAVOC_PER_CREW_FOOD_MILLI(v) foodMilli = (v);
#define HAVOC_PER_CREW_MEDICAL_MILLI(v) medicalMilli = (v);
#define HAVOC_UNIT(id, faction, crew, fuelCap, fuelBurn, ammo, maint) \
  d.units.push_back(UnitDef{id, faction, crew, Milli(fuelCap), Milli(fuelBurn), Milli(ammo), Milli(maint)});
#define HAVOC_FORMATION_UNIT(fid, faction, uid, count)
#include "../../generated/HavocData.inc"
#undef HAVOC_FORMATION_UNIT
#undef HAVOC_UNIT
  d.foodPerCrewPerDay = Milli(foodMilli);
  d.medicalPerCrewPerDay = Milli(medicalMilli);

  // Second pass for formations, now that unit indices are known.
#define HAVOC_UNIT(id, faction, crew, fuelCap, fuelBurn, ammo, maint)
#define HAVOC_FORMATION_UNIT(fid, faction, uid, count)                                   \
  {                                                                                      \
    if (d.formations.empty() || d.formations.back().id != (fid)) {                       \
      d.formations.push_back(FormationDef{fid, faction, {}});                            \
    }                                                                                    \
    for (std::size_t i = 0; i < d.units.size(); ++i) {                                   \
      if (d.units.at(i).id == (uid)) { d.formations.back().entries.push_back({i, (count)}); } \
    }                                                                                    \
  }
#include "../../generated/HavocData.inc"
#undef HAVOC_FORMATION_UNIT
#undef HAVOC_UNIT
#undef HAVOC_PER_CREW_MEDICAL_MILLI
#undef HAVOC_PER_CREW_FOOD_MILLI
  return d;
}

}  // namespace

const UnitDef* DataSet::FindUnit(const std::string& id) const {
  auto it = std::lower_bound(units.begin(), units.end(), id,
                             [](const UnitDef& u, const std::string& k) { return u.id < k; });
  return (it != units.end() && it->id == id) ? &*it : nullptr;
}

const FormationDef* DataSet::FindFormation(const std::string& id) const {
  auto it = std::lower_bound(formations.begin(), formations.end(), id,
                             [](const FormationDef& f, const std::string& k) { return f.id < k; });
  return (it != formations.end() && it->id == id) ? &*it : nullptr;
}

const DataSet& BuiltinData() {
  static const DataSet data = Build();
  return data;
}

}  // namespace havoc::ops
