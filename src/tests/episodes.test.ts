import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import {
  AegisSupportController, BrokenCitadelMission, Campaign, ContactManager, DroneController, E02_POINTS,
  MountainShieldMission, SilentCorridorMission, parseMissionDefinition, pickSurpriseEvent,
  type E02Carryover, type Episode03Data, type GardunMapData, type MissionEvents, type StrongholdData
} from '../index.js'
import { EventBus } from '../core/EventBus.js'

const json = (path: string): unknown => JSON.parse(readFileSync(path, 'utf8'))
const map = json('data/maps/gardun-city.json') as GardunMapData
const sectors = map.sectors.map(s => s.id)
const e02Def = () => parseMissionDefinition(json('data/missions/episode-02-silent-corridor.json'), sectors)
const e03 = json('data/missions/episode-03-broken-citadel.json') as Episode03Data
const stronghold = json('data/missions/operation-broken-citadel.json') as StrongholdData

function e02(events = new EventBus<MissionEvents>(), crewRecovered = true): SilentCorridorMission {
  return new SilentCorridorMission(e02Def(), new DroneController(), new ContactManager(), events, crewRecovered)
}

/** Plays Episode 02 up to the bridge choice. */
function toBridge(m: SilentCorridorMission): void {
  m.progress()                                            // briefing → form-up → relay
  assert.equal(m.state, 'E02_RELAY_KILO')
  assert.ok(m.placeRelayKilo())
  m.progress()
  assert.equal(m.state, 'E02_CORRIDOR_SCOUT')
  assert.ok(m.scoutCorridor([{ id: 'e02-roadblock', position: { x: 1650, y: 0, z: 1480 } }]))
  m.progress()                                            // advance → comms outage (blocked)
  assert.equal(m.state, 'E02_COMMS_OUTAGE')
  assert.equal(m.relays.classify(E02_POINTS.corridorEnd), 'LOST', 'outage jams the drone link')
  assert.equal(m.restoreRelayKilo({ x: 0, y: 0, z: 0 }), false, 'must be at the relay to restore it')
  assert.ok(m.restoreRelayKilo({ x: 610, y: 40, z: 905 }))
  m.progress()
  assert.equal(m.state, 'E02_BRIDGE_OBSTRUCTED')
}

test('mission definitions validate: reachable states, known sectors, fictional', () => {
  assert.doesNotThrow(e02Def)
  assert.throws(() => parseMissionDefinition({ ...e02Def(), fictional: false }), /fictional/)
  assert.throws(() => parseMissionDefinition({ ...e02Def(), sectors: ['nowhere'] }, sectors), /unknown sector nowhere/)
  const broken = { ...e02Def(), states: e02Def().states.map(s => s.id === 'E02_BRIEFING' ? { ...s, next: 'E02_MISSING' } : s) }
  assert.throws(() => parseMissionDefinition(broken), /unknown state E02_MISSING/)
})

test('Episode 02: relay, outage and riverbed detour with an ambush, full strength', () => {
  const events = new EventBus<MissionEvents>()
  const seen: string[] = []
  const blocked: string[] = []
  events.on('mission:state', e => seen.push(e.to))
  events.on('mission:blocked', e => blocked.push(...e.missing))
  const m = e02(events)
  assert.equal(m.scoutCorridor(), false, 'cannot scout before the relay step')
  toBridge(m)
  assert.ok(blocked.includes('relay_kilo_restored'))
  assert.deepEqual(m.runner.choices, ['riverbed_detour', 'drainage_infiltration'])
  m.chooseRoute('riverbed_detour')
  assert.equal(m.convoy.state, 'AMBUSHED')
  assert.equal(m.convoy.route, 'RIVERBED')
  assert.equal(m.remainingAmbush.length, 3)
  m.damageVehicle('nomad-2', 0.4)                         // damaged, not destroyed
  for (const id of m.remainingAmbush) m.clearAmbushContact(id)
  m.progress()
  assert.equal(m.state, 'E02_SOUTH_GATE_APPROACH')
  m.clearStaging(); m.progress()
  assert.equal(m.state, 'E02_STAGING_ESTABLISHED')
  assert.equal(m.finish(), 'E02_COMPLETE')
  assert.equal(m.runner.outcome, 'SUCCESS')
  assert.equal(seen.at(-1), 'E02_COMPLETE')
  assert.deepEqual(m.carryover(), { episode: 2, outcome: 'SUCCESS', crewDelivered: true, nomadsRemaining: 2, wardenOperational: true, route: 'RIVERBED', relaysOnline: 1 })
})

test('Episode 02: drainage infiltration keeps vehicles out of the tunnel; losses give a PARTIAL result', () => {
  const m = e02()
  toBridge(m)
  m.chooseRoute('drainage_infiltration')
  assert.equal(m.convoy.depart('RIVER_DRAINAGE'), false, 'no vehicle fits the drainage tunnel')
  m.clearBridge()                                         // too early: far bank not clear
  m.progress()
  assert.equal(m.state, 'E02_DRAINAGE_INFILTRATION')
  m.clearFarBank(); m.progress()
  assert.equal(m.state, 'E02_BRIDGE_CLEARED')
  m.damageVehicle('nomad-2', 1)                           // supplies truck destroyed
  assert.equal(m.state, 'E02_BRIDGE_CLEARED', 'losing supplies is not mission failure')
  m.clearBridge(); m.progress()
  m.clearStaging(); m.progress()
  assert.equal(m.finish(), 'E02_COMPLETE_WITH_LOSSES')
  assert.equal(m.carryover().nomadsRemaining, 1)
  assert.equal(m.carryover().route, 'RIVER_DRAINAGE')
})

test('Episode 02: losing the crew transport fails the mission; the checkpoint restores it', () => {
  const m = e02()
  toBridge(m)
  const cp = m.runner.checkpoint
  assert.equal(cp?.state, 'E02_BRIDGE_OBSTRUCTED')
  m.chooseRoute('riverbed_detour')
  m.damageVehicle('nomad-1', 1)
  assert.equal(m.state, 'E02_CONVOY_LOST')
  assert.equal(m.runner.outcome, 'FAILED')
  assert.ok(m.runner.restoreCheckpoint())
  assert.equal(m.state, 'E02_BRIDGE_OBSTRUCTED')
  assert.equal(m.convoy.crewSafe, true, 'vehicles are back as they were at the checkpoint')
  assert.equal(m.runner.outcome, undefined)
  assert.deepEqual(m.runner.choices, ['riverbed_detour', 'drainage_infiltration'])
})

test('Episode 02 without the Falcon crew (Mission 01 abandoned the recovery): fails only when every Nomad is lost', () => {
  const m = e02(undefined, false)
  toBridge(m)
  assert.equal(m.convoy.carriesCrew, false)
  m.damageVehicle('nomad-1', 1)
  assert.equal(m.state, 'E02_BRIDGE_OBSTRUCTED')
  m.damageVehicle('nomad-2', 1)
  assert.equal(m.state, 'E02_CONVOY_LOST')
})

const carry = (over: Partial<E02Carryover> = {}): E02Carryover => ({ episode: 2, outcome: 'SUCCESS', crewDelivered: true, nomadsRemaining: 2, wardenOperational: true, route: 'RIVERBED', relaysOnline: 1, ...over })
const e03m = (c?: E02Carryover, seed = 'match-1') => new BrokenCitadelMission(e03, stronghold, map, new AegisSupportController(new ContactManager()), c, seed)

test('Episode 03: five phases in order, campaign unlocks and one surprise event', () => {
  const m = e03m(carry())
  assert.equal(m.reinforcements, 30, '20 base + 5 per surviving Nomad')
  assert.equal(m.beginAssault(), 'E03_PHASE_1')
  assert.equal(m.captureObjective('operations_room'), false, 'later-phase objectives do not count yet')
  assert.ok(m.captureObjective('capture_airfield'))     // phase 1 needs either objective
  assert.equal(m.state, 'E03_PHASE_2')
  assert.equal(m.airSupportAvailable, true)
  assert.equal(m.captureObjective('secure_industrial_depot'), false, 'phase 1 is over')
  m.captureObjective('west_ridge_radio')
  assert.equal(m.state, 'E03_PHASE_2', 'phase 2 needs both relay sites')
  assert.equal(m.counterattackStrength, 1)
  m.captureObjective('government_relay')
  assert.equal(m.counterattackStrength, 0.6)
  assert.equal(m.state, 'E03_PHASE_3')
  assert.equal(m.surprise.active, pickSurpriseEvent(map.surpriseEvents, 'match-1'))
  for (const o of ['market_square', 'medical_station', 'municipal_ruins']) m.captureObjective(o)
  assert.equal(m.state, 'E03_PHASE_4')
  assert.equal(m.surprise.active, undefined, 'the surprise event ends with its phase')
  assert.equal(m.surprise.trigger(), undefined, 'only one surprise event per match')
  assert.equal(m.aegis.state, 'OFFLINE')
  m.captureObjective('mountain_fort')
  assert.equal(m.aegis.state, 'AVAILABLE', 'Mountain Fort brings Aegis online')
  m.captureObjective('east_power_station')
  for (const o of ['outer_gate', 'inner_courtyard', 'command_hall', 'operations_room']) m.captureObjective(o)
  assert.equal(m.state, 'E03_CITADEL_SECURED')
  assert.equal(m.runner.outcome, 'SUCCESS')
})

test('Episode 03: carryover matters; reinforcements running out fails back to the checkpoint', () => {
  const lean = e03m(carry({ crewDelivered: false, nomadsRemaining: 0 }))
  assert.equal(lean.reinforcements, 20)
  lean.beginAssault(); lean.captureObjective('capture_airfield')
  assert.equal(lean.airSupportAvailable, false, 'no Falcon crew, no air support')
  const cp = lean.runner.checkpoint
  assert.equal(cp?.state, 'E03_PHASE_2')
  lean.spendReinforcements(25)
  assert.equal(lean.state, 'E03_ASSAULT_FAILED')
  assert.ok(lean.runner.restoreCheckpoint())
  assert.equal(lean.state, 'E03_PHASE_2')
  assert.equal(lean.reinforcements, 20)
  assert.deepEqual(lean.capturedObjectives, ['capture_airfield'])
})

test('surprise event is chosen deterministically from the seed', () => {
  assert.equal(pickSurpriseEvent(map.surpriseEvents, 'abc'), pickSurpriseEvent(map.surpriseEvents, 'abc'))
  const picks = new Set(Array.from({ length: 40 }, (_, i) => pickSurpriseEvent(map.surpriseEvents, `seed-${i}`)))
  assert.ok(picks.size > 1)
})

test('Campaign: episodes unlock in order and pass their results forward', () => {
  const c = new Campaign()
  assert.equal(c.next()?.episode, 1)
  assert.equal(c.isUnlocked(2), false)
  const m01 = new MountainShieldMission(new DroneController(), new AegisSupportController(new ContactManager()))
  m01.transition('M01_COUNTERATTACK')
  m01.abandonCrashRecovery()
  m01.transition('M01_COMPLETE')
  const r1 = Campaign.carryoverFromMission01(m01)
  assert.deepEqual(r1, { episode: 1, outcome: 'PARTIAL', crewRecovered: false })
  c.record(1, r1.outcome, r1)
  assert.equal(c.isUnlocked(2), true)
  assert.equal(c.crewRecoveredInMission01(), false)
  c.record(2, 'FAILED', carry({ outcome: 'FAILED' }))
  assert.equal(c.isUnlocked(3), false)
  c.record(2, 'PARTIAL', carry({ outcome: 'PARTIAL', nomadsRemaining: 1 }))
  c.record(2, 'FAILED', carry({ outcome: 'FAILED' }))       // a worse replay doesn't overwrite
  assert.equal(c.outcome(2), 'PARTIAL')
  assert.equal(c.next()?.episode, 3)
  assert.equal(Campaign.load(c.save()).episode02()?.nomadsRemaining, 1)
})
