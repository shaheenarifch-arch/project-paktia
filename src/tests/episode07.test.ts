import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { parseMissionDefinition } from '../gameplay/missions/framework/MissionDefinition.js'
import { CityOfFreedomMission, type Episode07Rules } from '../gameplay/missions/episode07/CityOfFreedom.js'

const raw = JSON.parse(readFileSync('data/missions/episode-07-city-of-freedom.json', 'utf8')) as Record<string, unknown> & Episode07Rules
const map = JSON.parse(readFileSync('data/maps/city-of-freedom.json', 'utf8')) as { sectors: { id: string }[] }
const def = () => parseMissionDefinition(raw, map.sectors.map(s => s.id))
const mission = () => new CityOfFreedomMission(def(), raw)

function toDomeSquare(m: CityOfFreedomMission, route: 'river_bridges' | 'arcade_route' = 'river_bridges'): void {
  m.progress()
  for (const s of m.squads) assert.ok(m.linkUp(s.id))
  m.progress(); assert.equal(m.state, 'E07_OLD_BAZAAR')
  m.secureBazaar(); m.progress()
  assert.equal(m.shelterCivilians('parvaneh_courtyard_quarter', 60), 60)
  m.progress()
  m.chooseCrossing(route); m.secureCrossing(); m.progress()
  assert.equal(m.state, 'E07_DOME_SQUARE')
}

function finishAll(m: CityOfFreedomMission): void {
  m.secureSquare(); m.progress(); m.secureCompound(); m.progress()
  assert.equal(m.state, 'E07_DEBRIEF')
}

test('Episode 07 is a declared real-world hypothetical with a fictional city; flags must agree', () => {
  assert.equal(def().setting, 'real_world_hypothetical')
  assert.equal(map.sectors.length, 8)
  assert.throws(() => parseMissionDefinition({ ...raw, fictional: true }), /must set fictional: false/)
})

test('clean run with side objectives is a SUCCESS', () => {
  const m = mission()
  toDomeSquare(m)
  for (let i = 0; i < 12; i++) m.treatPatient()
  for (let i = 0; i < 3; i++) m.deliverAidDrop()
  finishAll(m)
  assert.equal(m.finish(), 'E07_CITY_FREED')
  const r = m.result()
  assert.equal(r.outcome, 'SUCCESS'); assert.equal(r.route, 'BRIDGES')
  assert.equal(r.heartsAndMinds, 64); assert.equal(r.civilianHarm, 0)
})

test('ROE: civilian harm fails the active objective and lowers Hearts & Minds; result is PARTIAL', () => {
  const m = mission()
  assert.equal(m.reportViolation('CIVILIAN_HARM'), false, 'ROE is not in force during the briefing')
  toDomeSquare(m, 'arcade_route')
  assert.ok(m.reportViolation('CIVILIAN_HARM', 'parvaneh_dome_square'))
  assert.equal(m.roe.heartsAndMinds, 35)
  assert.deepEqual(m.roe.failedObjectives, ['E07_DOME_SQUARE'])
  for (let i = 0; i < 12; i++) m.treatPatient()
  for (let i = 0; i < 3; i++) m.deliverAidDrop()
  finishAll(m)
  assert.equal(m.finish(), 'E07_CITY_FREED_PARTIAL')
})

test('ROE: surrendered soldiers are protected; three violations fail the operation; checkpoint restores', () => {
  const m = mission()
  toDomeSquare(m)
  assert.equal(m.runner.checkpoint?.state, 'E07_DOME_SQUARE')
  m.reportViolation('SURRENDERED_HARM')
  assert.equal(m.roe.heartsAndMinds, 40)
  m.reportViolation('CIVILIAN_HARM'); m.reportViolation('SURRENDERED_HARM')
  assert.equal(m.state, 'E07_OPERATION_FAILED')
  assert.ok(m.runner.restoreCheckpoint())
  assert.equal(m.state, 'E07_DOME_SQUARE')
  assert.equal(m.roe.violations.length, 0, 'violations after the checkpoint are undone')
  assert.equal(m.roe.heartsAndMinds, 50)
})

test('running out of reinforcements fails; resistance losses count', () => {
  const m = mission()
  m.progress()
  m.resistanceLost('lantern_squad', 99)
  assert.equal(m.squads[0]!.lost, 6)
  m.spendReinforcements(100)
  assert.equal(m.state, 'E07_OPERATION_FAILED')
})
