import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { AegisSupportController, Campaign, ContactManager, E04_PROTECTED, OpenStraitMission, parseMissionDefinition, type Episode04Rules } from '../index.js'

const raw = JSON.parse(readFileSync('data/missions/episode-04-open-strait.json', 'utf8')) as Record<string, unknown> & Episode04Rules
const map = JSON.parse(readFileSync('data/maps/kharg-hormuz.json', 'utf8')) as { sectors: { id: string }[] }
const def = () => parseMissionDefinition(raw, map.sectors.map(s => s.id))

function mission(e3: 'SUCCESS' | 'PARTIAL' | undefined = 'SUCCESS') {
  const contacts = new ContactManager()
  return new OpenStraitMission(def(), raw, new AegisSupportController(contacts), contacts, e3)
}

function takeIsland(m: OpenStraitMission, beach: 'north_beach' | 'west_beach'): void {
  m.progress()
  assert.equal(m.state, 'E04_BEACH_RECON')
  assert.ok(m.scoutBeaches({ north: 8, west: 3 }))
  m.chooseBeach(beach)
  m.secureBeachhead(); m.progress()
  if (beach === 'north_beach') { m.secureTerminal(); m.progress(); m.secureAirstrip(); m.progress() }
  else { m.secureAirstrip(); m.progress(); m.secureTerminal(); m.progress() }
  assert.equal(m.state, 'E04_LANE_CLEARANCE')
}

function clearLanes(m: OpenStraitMission): void {
  assert.equal(m.clearHazard('inbound'), false, 'hazards must be revealed before clearing')
  for (const lane of ['inbound', 'outbound'] as const) {
    m.revealHazards(lane, 10)
    while (m.clearHazard(lane)) { /* clear every revealed hazard */ }
  }
  m.progress()
  assert.equal(m.state, 'E04_TANKER_ESCORT')
}

test('Episode 04 is declared a real-world hypothetical; the fictional flag must agree', () => {
  assert.equal(def().setting, 'real_world_hypothetical')
  assert.throws(() => parseMissionDefinition({ ...raw, fictional: true }), /must set fictional: false/)
})

test('north beach: terminal first, lanes cleared, tankers through, strait open (SUCCESS)', () => {
  const m = mission('SUCCESS')
  assert.equal(m.reinforcements, 40, '30 + 10 for winning Episode 03')
  takeIsland(m, 'north_beach')
  assert.equal(m.reinforcements, 32, 'north beach defences cost 8')
  assert.equal(m.airSupport, true)
  assert.equal(m.startTransit('tanker-1', 'inbound'), false, 'no transits before the escort phase')
  clearLanes(m)
  assert.equal(m.escort.tankers.length, 6)
  for (const t of m.escort.tankers) { assert.ok(m.startTransit(t.id, 'outbound')); m.tankerArrived(t.id) }
  assert.equal(m.state, 'E04_DEBRIEF')
  assert.equal(m.finish(), 'E04_STRAIT_OPEN')
  assert.deepEqual(m.result(), { episode: 4, outcome: 'SUCCESS', beach: 'NORTH', terminalIntegrity: 1, canExport: true, tankersThrough: 6, tankersLost: 0, supplyFlow: 1 })
})

test('west beach with a damaged terminal and lost tankers ends PARTIAL', () => {
  const m = mission('PARTIAL')
  assert.equal(m.reinforcements, 30, 'no bonus without a full Episode 03 win')
  takeIsland(m, 'west_beach')
  assert.equal(m.result().beach, 'WEST')
  m.damageTerminal('tank_farm', 0.9)
  m.damageTerminal('east_jetty', 0.5)
  clearLanes(m)
  const [a, b, ...rest] = m.escort.tankers.map(t => t.id)
  m.startTransit(a!, 'inbound'); m.damageTanker(a!, 1)
  m.startTransit(b!, 'inbound'); m.damageTanker(b!, 1)
  for (const id of rest) { m.startTransit(id, 'outbound'); m.tankerArrived(id) }
  assert.equal(m.finish(), 'E04_STRAIT_OPEN_PARTIAL')
  const r = m.result()
  assert.equal(r.tankersLost, 2)
  assert.ok(r.supplyFlow < 0.8 && r.terminalIntegrity < 0.8)
})

test('protected zones cover the terminal and town; losing every tanker fails; checkpoints restore', () => {
  const m = mission()
  const contacts = m.contacts
  contacts.upsert({ id: 'in-terminal', classification: 'GAME_UNIT', position: E04_PROTECTED[0]!.position, source: 'RAVEN', time: 1, quality: 1 })
  contacts.confirm('in-terminal')
  m.aegis.setOnline(true)
  assert.deepEqual(m.aegis.selectTarget('in-terminal'), { valid: false, reason: 'PROTECTED_ZONE' })
  takeIsland(m, 'north_beach')
  clearLanes(m)
  assert.equal(m.runner.checkpoint?.state, 'E04_TANKER_ESCORT')
  for (const t of m.escort.tankers) { m.startTransit(t.id, 'inbound'); m.damageTanker(t.id, 1) }
  assert.equal(m.state, 'E04_OPERATION_FAILED')
  assert.ok(m.runner.restoreCheckpoint())
  assert.equal(m.state, 'E04_TANKER_ESCORT')
  assert.equal(m.lanes.allOpen, true, 'lane work done before the checkpoint is kept')
  assert.ok(m.escort.tankers.every(t => t.state === 'WAITING'), 'the tankers are back in harbour')
})

test('Campaign: Episode 04 unlocks after Episode 03', () => {
  const c = new Campaign()
  for (const ep of [1, 2]) c.record(ep, 'SUCCESS', {})
  assert.equal(c.isUnlocked(4), false)
  c.record(3, 'PARTIAL', {})
  assert.equal(c.isUnlocked(4), true)
  assert.equal(c.next()?.id, 'episode_04_open_strait')
})
