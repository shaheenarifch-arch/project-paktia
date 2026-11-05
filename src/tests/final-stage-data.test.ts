import test from 'node:test'
import assert from 'node:assert/strict'
import { readFileSync } from 'node:fs'
import { parseMissionDefinition } from '../index.js'

const json = (path: string) => JSON.parse(readFileSync(path, 'utf8')) as any
const EPISODES = [
  { file: 'data/missions/episode-05-first-light.json', map: 'data/maps/first-light-highlands.json' },
  { file: 'data/missions/episode-06-broken-chains.json', map: 'data/maps/shahrbaran-city.json' },
  { file: 'data/missions/episode-07-long-night.json', map: 'data/maps/gulshar-capital.json' },
  { file: 'data/missions/episode-08-open-hand.json', map: 'data/maps/homeland-handover.json' }
]
const faction = json('data/factions/iran-resistance.json')
const resistanceUnits = json('data/units/iran-resistance.json')
const regimeUnits = json('data/units/iran-regime-security.json')
const unitFaction = new Map<string, string>([...resistanceUnits.units, ...regimeUnits.units, ...json('data/units/iran.json').units].map((u: any) => [u.id, u.faction]))
const formations = new Map<string, any>([...json('data/formations/iran-resistance.json').formations, ...json('data/formations/iran-regime-security.json').formations].map((f: any) => [f.id, f]))
const hm = json('data/final-stage/hearts-minds.json')
const sd = json('data/final-stage/surrender-defection.json')

test('every final-stage mission parses, is a declared hypothetical and sits on a matching fictional map', () => {
  EPISODES.forEach(({ file, map }, i) => {
    const raw = json(file)
    const m = json(map)
    const def = parseMissionDefinition(raw, m.sectors.map((s: any) => s.id))
    assert.equal(def.episode, 5 + i)
    assert.equal(def.setting, 'real_world_hypothetical')
    assert.equal(def.fictional, false)
    assert.equal(m.setting, 'real_world_hypothetical')
    assert.equal(m.fictional, false)
    assert.equal(raw.map, m.id)
    assert.ok(def.failState, 'needs a fail state')
    assert.ok(def.states.some(s => s.terminal && s.outcome === 'SUCCESS'))
    assert.ok(def.states.some(s => s.terminal && s.outcome === 'FAILED'))
    assert.ok(def.states.filter(s => s.checkpoint).length >= 3, 'needs checkpoints')
    assert.equal(raw.civilianHarmFailCount, hm.civilianHarmFailCount)
  })
})

test('episodes chain: each is unlocked by the one before', () => {
  const ids = ['episode_04_open_strait', ...EPISODES.map(e => json(e.file).id)]
  EPISODES.forEach((e, i) => assert.equal(json(e.file).unlockedBy, ids[i]))
})

test('episode 08 selects an ending for each Hearts & Minds ending state', () => {
  const raw = json(EPISODES[3]!.file)
  const states = new Map(raw.states.map((s: any) => [s.id, s]))
  assert.deepEqual(raw.endings, hm.endings.map((e: any) => e.id))
  for (const end of hm.endings) {
    const s = states.get(end.state) as any
    assert.ok(s?.terminal, `${end.state} must be a terminal state`)
    assert.equal(s.outcome, end.outcome)
    assert.equal((states.get('E08_ENDING') as any).branches[end.id], end.state)
  }
})

test('iran-resistance is an invented, playable ally and never backed by the regime', () => {
  assert.equal(faction.id, 'iran-resistance')
  assert.equal(faction.playable, true)
  assert.equal(faction.setting, 'real_world_hypothetical')
  assert.equal(faction.fictional, false)
  assert.equal(faction.backedBy, undefined)
})

test('new units and formations stay in their factions and use no real designations', () => {
  assert.equal(resistanceUnits.faction, 'iran-resistance')
  assert.equal(regimeUnits.faction, 'iran')
  for (const u of [...resistanceUnits.units, ...regimeUnits.units]) {
    assert.equal(u.faction, u.id.startsWith('iran_resistance_') ? 'iran-resistance' : 'iran', u.id)
    assert.match(u.id, /^iran_(resistance|regime)_/)
  }
  for (const f of formations.values()) {
    for (const uid of Object.keys(f.units)) assert.equal(unitFaction.get(uid), f.faction, `${f.id}: ${uid}`)
  }
})

test('Hearts & Minds spec is complete and consistent', () => {
  assert.ok(hm.score.min < hm.score.start && hm.score.start < hm.score.max)
  assert.ok(Object.keys(hm.gains).length >= 3)
  for (const k of ['civilian_harmed', 'surrendered_fired_upon']) assert.ok(hm.penalties[k].points < 0)
  for (const g of Object.values<any>(hm.gains)) assert.ok(g.points > 0 && g.perEpisodeCap > 0)
  const tiers = hm.supportTiers.map((t: any) => t.minScore)
  assert.deepEqual(tiers, [...tiers].sort((a, b) => a - b))
  for (const t of hm.supportTiers) for (const f of t.unlocks.formations) assert.equal(formations.get(f)?.faction, 'iran-resistance', f)
  const scores = hm.endings.map((e: any) => e.minScore)
  assert.deepEqual(scores, [...scores].sort((a, b) => b - a))
  assert.equal(scores[scores.length - 1], hm.score.min, 'the lowest ending must catch every score')
  assert.ok(hm.endings.length >= 3)
})

test('surrender/defection spec references real units and defectors join the resistance', () => {
  const seen = new Set<string>()
  for (const u of sd.units) {
    assert.equal(unitFaction.get(u.unit), 'iran', u.unit)
    assert.ok(!seen.has(u.unit)); seen.add(u.unit)
    assert.ok(['may_surrender', 'may_defect', 'never'].includes(u.disposition))
    assert.ok(u.baseAccept >= 0 && u.baseAccept <= 1 && u.defectShare >= 0 && u.defectShare <= 1)
    if (u.disposition === 'never') assert.equal(u.baseAccept, 0)
    if (u.defectShare > 0) assert.equal(unitFaction.get(u.defectsInto), 'iran-resistance')
  }
  assert.equal(sd.protection.surrenderedUnitsAreNotTargets, true)
  assert.ok(hm.penalties[sd.protection.firingOnSurrenderPenalty])
  assert.ok(hm.penalties[sd.protection.unattendedPrisonerPenalty])
  for (const o of Object.values<any>(sd.outcomes)) if (o.heartsMindsEvent) assert.ok(hm.gains[o.heartsMindsEvent], o.heartsMindsEvent)
})

test('data stays inside the safety boundaries: no real people, no instructions', () => {
  const files = [...EPISODES.flatMap(e => [e.file, e.map]), 'data/factions/iran-resistance.json', 'data/units/iran-resistance.json', 'data/units/iran-regime-security.json',
    'data/formations/iran-resistance.json', 'data/formations/iran-regime-security.json', 'data/final-stage/hearts-minds.json', 'data/final-stage/surrender-defection.json']
  for (const f of files) {
    const text = readFileSync(f, 'utf8')
    assert.doesNotMatch(text, /khamenei|raisi|rouhani|pahlavi|soleimani|trump|biden|reza/i, `${f} must not name real people`)
    assert.doesNotMatch(text, /\b(how to build|assembly instructions|step-by-step|coordinates|lat\/lon)\b/i, `${f} must not hold instructions`)
  }
})
