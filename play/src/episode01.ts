import * as THREE from 'three'
import {
  AegisSupportController, Campaign, ContactManager, DroneController, MissionRunner, MountainShieldMission, parseMissionDefinition,
  type M01Carryover, type MissionDefinition, type MissionOutcome
} from '../../src/index.js'
import episodeJson from '../../data/missions/episode-01-mountain-shield.json'
import mapJson from '../../data/maps/paktia-highlands.json'
import { Collision, PALETTE, Solids, groundPatch, rng } from './collision'
import { helicopterMesh } from './episode03'
import { BASE_CONTROLS, controlsHtml, type HudFrame } from './hud'
import { ScenarioBattle, type BattleOptions } from './scenario'

// Episode 01, Operation Mountain Shield, in the Paktia Highlands (fictional).
// The state graph is data/missions/episode-01-mountain-shield.json, run by
// MissionRunner. MountainShieldMission keeps the Ridge Station, Aegis and
// Falcon-2 crash systems in step so the campaign carryover comes from the
// same code the tests cover.

const DEF = parseMissionDefinition(episodeJson, (mapJson as { sectors: { id: string }[] }).sectors.map(s => s.id)) as MissionDefinition
export const HALF = 250
const BOUNDS = { x0: -HALF, z0: -HALF, x1: HALF, z1: HALF }

const START = { x: 0, z: 225 }
const ECHO = { x: 0, z: 120 }
const TUTORIAL_BEACON = { x: 70, z: 20 }
const RIDGE = { x: 150, z: -70 }
const CRASH = { x: -170, z: -150 }
const EVAC = { x: -30, z: 170 }
const CREW = [{ id: 'pilot', x: -186, z: -138 }, { id: 'copilot', x: -158, z: -168 }, { id: 'chief', x: -180, z: -118 }]
const E = { holdSeconds: 70, waveEvery: 20, echoDrain: 1.4, echoRegen: 1.5, ridgeDrain: 1.2 }

export interface Episode01Options extends BattleOptions {
  onResult: (outcome: MissionOutcome, carryover: M01Carryover) => void
}

export class MountainShieldGame extends ScenarioBattle {
  protected runner!: MissionRunner
  private mission!: MountainShieldMission
  private readonly o: Episode01Options
  private echo = 100
  private ridge = 100
  private holdT = 0
  private waveT = 0
  private wavesSpawned = 0
  private picked = new Set<string>()
  private falcon: { mesh: THREE.Group; t: number } | null = null
  private tutorial = { flew: false }
  private ended = false
  private ridgeCaptured = false

  constructor(root: HTMLElement, o: Episode01Options) {
    super(root, o)
    this.o = o
    buildMap(this.scene, c => { this.collision = c })
    Object.assign(this.player, { x: START.x, z: START.z })
    this.initRaven()
    this.raven.x = START.x; this.raven.z = START.z
    this.hud.controls = controlsHtml([...BASE_CONTROLS, [['Q'], 'Fly Raven / back to the squad'], [['1', '2'], 'Pick at the crash decision']])
    this.newMission()
    this.runner.events.on('mission:state', ({ to }) => this.onState(to))
    this.runner.events.on('mission:complete', ({ outcome }) => this.finish(outcome))
    this.start()
    this.briefing({
      eyebrow: 'OPERATION MIDNIGHT HAVOC // EPISODE 01', title: 'Operation Mountain Shield',
      text: 'Fictional Paktia Highlands. Learn the Raven drone, hold Checkpoint Echo, set up Ridge Station, then Falcon-2 is shot down: recover the crew or abandon them, and survive the counterattack.',
      button: 'Begin'
    }, () => this.onState(this.runner.state))
  }

  // ---------- mission ----------

  private newMission(): void {
    this.mission = new MountainShieldMission(new DroneController(), new AegisSupportController(new ContactManager()))
    this.runner = new MissionRunner(DEF, {
      'ridge.discover': () => this.mission.ridgeStation.discover(),
      'crash.begin': () => { this.mission.transition('M01_COUNTERATTACK'); this.syncTo('M01_HELICOPTER_CRASH') },
      'crew.locate': () => this.syncTo('M01_CRASH_SITE_LOCATED'),
      'crew.abandon': () => this.mission.abandonCrashRecovery(),
      'counterattack.begin': () => this.mission.counterattack.begin()
    })
  }

  /** Steps the crash sequence forward until the domain reaches `target`. */
  private syncTo(target: Parameters<MountainShieldMission['transition']>[0]): void {
    for (let i = 0; i < 14 && this.mission.state !== target; i++) {
      const before = this.mission.state
      this.mission.continueCrashSequence()
      if (this.mission.state === before) break
    }
  }

  /** After a checkpoint restore the domain objects are rebuilt to match the state graph's history. */
  private rebuildDomain(): void {
    const h = this.runner.history
    this.mission = new MountainShieldMission(new DroneController(), new AegisSupportController(new ContactManager()))
    this.ridgeCaptured = false
    if (h.includes('M01_RIDGE_STATION')) this.mission.ridgeStation.discover()
    if (this.runner.hasFlag('ridge_station_set')) { this.mission.transition('M01_RIDGE_STATION_CAPTURED'); this.ridgeCaptured = true }
    if (h.includes('M01_FALCON_DOWN')) { this.mission.transition('M01_COUNTERATTACK'); this.syncTo('M01_HELICOPTER_CRASH') }
    if (h.includes('M01_CREW_RECOVERY')) this.syncTo('M01_CRASH_SITE_LOCATED')
    if (h.includes('M01_ABANDONED')) this.mission.abandonCrashRecovery()
    if (h.includes('M01_COUNTERATTACK')) this.mission.counterattack.begin()
  }

  private onState(state: string): void {
    this.clearSites()
    const toast = (t: string, kind: 'info' | 'good' | 'phase' = 'phase') => this.hud.toast(t, kind, 4.5)
    switch (state) {
      case 'M01_DRONE_TUTORIAL':
        this.tutorial = { flew: false }
        toast('Tutorial: press Q to launch Raven, fly to the amber beacon, then Q to return.')
        break
      case 'M01_HOLD_ECHO':
        this.echo = 100; this.holdT = E.holdSeconds; this.waveT = 6; this.wavesSpawned = 0
        this.respawnAt = ECHO
        toast('Hold Checkpoint Echo. Keep hostiles out of the compound.')
        break
      case 'M01_RIDGE_STATION':
        this.clearHostiles()
        this.respawnAt = { x: 20, z: 90 }
        this.group(5, RIDGE.x, RIDGE.z, 22, RIDGE, 40, 'ridge')
        this.addSite({ id: 'ridge', x: RIDGE.x, z: RIDGE.z, r: 14, label: 'Ridge Station', seconds: 8 })
        toast('Move to Ridge Station. Clear the garrison and hold the ring.')
        break
      case 'M01_FALCON_DOWN': {
        this.respawnAt = { x: RIDGE.x - 30, z: RIDGE.z + 30 }
        const mesh = helicopterMesh(); mesh.position.set(RIDGE.x, 40, -HALF + 10); this.scene.add(mesh)
        this.falcon = { mesh, t: 0 }
        toast('Falcon-2 is under fire!')
        break
      }
      case 'M01_CRASH_DECISION':
        this.choose('DECISION · FALCON-2 DOWN', 'Grid R-17', this.runner.objective, [
          { label: 'Recover the crew', detail: 'Fight to the wreck, carry all three crew to the evacuation point and survive the recovery defence. Full result.', pick: () => this.runner.choose('recover_crew') },
          { label: 'Abandon the crew', detail: 'Fall back to Ridge Station and hold. A safer path, but the campaign result is only partial.', pick: () => this.runner.choose('abandon_crew') }
        ])
        break
      case 'M01_CREW_RECOVERY':
        this.picked.clear()
        this.clearHostiles()
        this.respawnAt = { x: -100, z: -60 }
        this.group(6, CRASH.x, CRASH.z, 28, CRASH, 50, 'r17')
        this.addSite({ id: 'r17', x: CRASH.x, z: CRASH.z, r: 20, label: 'Grid R-17', seconds: 8 })
        toast('Secure grid R-17, then reach each crew member.')
        break
      case 'M01_COUNTERATTACK':
        this.ridge = 100; this.wavesSpawned = 0; this.waveT = 4
        this.respawnAt = { x: RIDGE.x - 30, z: RIDGE.z + 40 }
        this.hud.toast(this.runner.history.includes('M01_ABANDONED') ? 'The crew is left behind. Hold Ridge Station.' : 'Crew home. Now hold Ridge Station.', 'phase', 4.5)
        break
      case 'M01_DEBRIEF':
        this.runner.choose(this.mission.helicopterCrash.controller.state === 'EVACUATION_COMPLETE' ? 'crew_recovered' : 'crew_abandoned')
        break
    }
  }

  private respawnAt = { ...START }
  protected respawnPoint(): { x: number; z: number } { return { ...this.respawnAt } }

  protected onSite(id: string): void {
    const r = this.runner
    if (id === 'ridge') {
      this.mission.transition('M01_RIDGE_STATION_CAPTURED'); this.ridgeCaptured = true
      this.hud.toast('Ridge Station relay is up. Aegis online.', 'good', 4)
      r.setFlag('ridge_station_set')
    } else if (id === 'r17') {
      this.syncTo('M01_CRASH_SITE_SECURE'); this.syncTo('M01_CREW_RECOVERY')
      this.hud.toast('Site secure. Reach the crew: pilot, co-pilot, crew chief.', 'good', 4)
      for (const c of CREW) this.addSite({ id: `crew:${c.id}`, x: c.x, z: c.z, r: 9, label: c.id, seconds: 3 })
    } else if (id.startsWith('crew:')) {
      this.picked.add(id.slice(5))
      if (this.picked.size === 1) {
        this.syncTo('M01_RECOVERY_DEFENSE')
        this.hud.toast('Hostiles are closing on the wreck. Get the crew out!', 'warn', 4)
        this.group(5, CRASH.x + 40, CRASH.z + 60, 20, CRASH, 160, 'defence')
      }
      this.hud.toast(`${this.picked.size}/3 crew with you`, 'good', 2.5)
      if (this.picked.size === 3) {
        this.addSite({ id: 'evac', x: EVAC.x, z: EVAC.z, r: 16, label: 'Evacuation point', seconds: 5 })
        this.hud.toast('Carry the crew to the evacuation point.', 'phase', 4)
        this.group(4, -100, 40, 20, { x: -100, z: 60 }, 120, 'defence')
      }
    } else if (id === 'evac') {
      this.syncTo('M01_EVAC_COMPLETE')
      this.hud.toast('Crew evacuated.', 'good', 4)
      r.setFlag('crew_evacuated')
    }
  }

  private finish(outcome: MissionOutcome): void {
    if (this.ended) return
    if (outcome === 'FAILED') {
      this.o.onResult('FAILED', { episode: 1, outcome: 'FAILED', crewRecovered: false })
      this.failCard('Position lost', this.runner.objective)
      return
    }
    this.ended = true
    this.mission.transition('M01_COMPLETE')
    const carry = Campaign.carryoverFromMission01(this.mission)
    this.o.onResult(outcome, carry)
    this.winCard({
      eyebrow: 'OPERATION COMPLETE', title: outcome === 'SUCCESS' ? 'Mountain Shield holds' : 'Mountain Shield holds, crew left behind',
      text: this.runner.objective, good: outcome === 'SUCCESS',
      stats: [{ label: 'Falcon-2 crew', value: carry.crewRecovered ? 'recovered' : 'not recovered' }]
    })
  }

  protected restoreCheckpoint(): void {
    if (!this.runner.checkpoint) return
    this.clearHostiles(); this.clearBullets(); this.clearSites()
    if (this.falcon) { this.scene.remove(this.falcon.mesh); this.falcon = null }
    this.raven.flying = false
    this.resume()
    this.ended = false
    this.runner.restoreCheckpoint()
    this.rebuildDomain()
    this.respawn()
    this.hud.toast('Restored from checkpoint', 'good', 2.5)
  }

  // ---------- frame ----------

  protected update(dt: number): void {
    const r = this.runner, st = r.state
    this.updateRaven(dt, BOUNDS)
    this.updateSites(dt)
    if (this.falcon) this.updateFalcon(dt)

    if (st === 'M01_DRONE_TUTORIAL') {
      if (this.raven.flying && Math.hypot(this.raven.x - TUTORIAL_BEACON.x, this.raven.z - TUTORIAL_BEACON.z) < 14 && !this.tutorial.flew) {
        this.tutorial.flew = true
        this.hud.toast('Contact confirmed. Press Q to return to the squad.', 'good', 4)
      }
      if (this.tutorial.flew && !this.raven.flying) r.setFlag('tutorial_complete')
    } else if (st === 'M01_HOLD_ECHO') {
      this.holdT -= dt
      const near = this.countWithin(ECHO.x, ECHO.z, 24)
      const defended = Math.hypot(this.player.x - ECHO.x, this.player.z - ECHO.z) < 26 && this.player.alive
      if (near > 0) this.echo -= near * E.echoDrain * (defended ? 0.4 : 1) * dt
      else this.echo = Math.min(100, this.echo + E.echoRegen * dt)
      this.spawnWaves(dt, 20, 4, i => ({ x: ECHO.x + (i % 2 ? 90 : -90), z: ECHO.z - 110 }), ECHO, 200)
      if (this.echo <= 0) { r.fail(); return }
      if (this.holdT <= 0) { this.hud.toast('Checkpoint Echo held.', 'good', 3); r.setFlag('echo_held') }
    } else if (st === 'M01_FALCON_DOWN') {
      // handled in updateFalcon
    } else if (st === 'M01_COUNTERATTACK') {
      const near = this.countWithin(RIDGE.x, RIDGE.z, 26)
      const defended = Math.hypot(this.player.x - RIDGE.x, this.player.z - RIDGE.z) < 28 && this.player.alive
      if (near > 0) this.ridge -= near * E.ridgeDrain * (defended ? 0.4 : 1) * dt
      else this.ridge = Math.min(100, this.ridge + E.echoRegen * dt)
      this.spawnWaves(dt, 3, 4, i => ({ x: RIDGE.x + (i % 2 ? -110 : 40), z: RIDGE.z - 100 }), RIDGE, 220, 2)
      if (this.ridge <= 0) { r.fail(); return }
      if (this.wavesSpawned >= 3 && this.alive().length === 0) {
        this.mission.counterattack.repel()
        this.hud.toast('Counterattack repelled.', 'good', 3)
        r.setFlag('counterattack_repelled')
      }
    }
    this.autoAdvance()
  }

  /** Timed attack waves aimed at a defended point. */
  private spawnWaves(dt: number, maxWaves: number, base: number, from: (i: number) => { x: number; z: number }, anchor: { x: number; z: number }, leash: number, step = 1): void {
    if (this.wavesSpawned >= maxWaves) return
    if ((this.waveT -= dt) > 0) return
    this.waveT = E.waveEvery
    const i = this.wavesSpawned++
    const at = from(i)
    this.group(base + i * step, at.x, at.z, 18, anchor, leash, 'wave')
    this.hud.toast(`Wave ${i + 1}: hostiles inbound`, 'warn', 3)
  }

  private updateFalcon(dt: number): void {
    const f = this.falcon!
    f.t += dt
    const k = Math.min(1, f.t / 7)
    f.mesh.position.set(RIDGE.x + (CRASH.x - RIDGE.x) * k, 40 - 36 * k * k, -HALF + 10 + (CRASH.z + HALF - 10) * k)
    f.mesh.rotation.y = Math.atan2(CRASH.x - RIDGE.x, CRASH.z + HALF)
    f.mesh.rotation.z = k * 0.9
    if (Math.random() < 0.5) this.puff(f.mesh.position.x, f.mesh.position.z, 0x222222, 2)
    if (k >= 1) {
      this.puff(CRASH.x, CRASH.z, 0xff9b3d, 8)
      f.mesh.position.y = 1.2
      this.falcon = null
      this.hud.toast('Falcon-2 is down at grid R-17.', 'warn', 4)
      this.runner.setFlag('crash_seen')
    }
  }

  protected override pauseCard() {
    const card = super.pauseCard()
    return { ...card, title: 'Operation Mountain Shield', text: this.runner.objective }
  }

  protected hudFrame(): HudFrame {
    const st = this.runner.state
    const items: HudFrame['items'] = []
    const vitals: HudFrame['vitals'] = []
    let hint: string | null = null
    if (st === 'M01_DRONE_TUTORIAL') {
      items.push({ label: 'Launch Raven (Q)', done: this.tutorial.flew || this.raven.flying }, { label: 'Reach the beacon', done: this.tutorial.flew }, { label: 'Return to the squad (Q)', done: false })
      hint = this.raven.flying ? 'Fly Raven (WASD) to the amber beacon.' : this.tutorial.flew ? 'Press Q to return.' : 'Press Q to launch Raven.'
    } else if (st === 'M01_HOLD_ECHO') {
      items.push({ label: `Hold Checkpoint Echo (${Math.max(0, Math.ceil(this.holdT))}s)`, done: false, progress: 1 - Math.max(0, this.holdT) / E.holdSeconds })
      vitals.push({ label: 'ECHO', bar: this.echo / 100, low: this.echo < 35 })
    } else if (st === 'M01_RIDGE_STATION') {
      items.push({ label: 'Ridge Station', done: false, progress: this.siteProgress('ridge') })
    } else if (st === 'M01_CREW_RECOVERY') {
      items.push({ label: 'Secure grid R-17', done: !this.sites.has('r17') }, ...CREW.map(c => ({ label: `Crew: ${c.id}`, done: this.picked.has(c.id) })), { label: 'Evacuation point', done: false, progress: this.siteProgress('evac') })
    } else if (st === 'M01_COUNTERATTACK') {
      items.push({ label: `Repel the counterattack (${Math.min(3, this.wavesSpawned)}/3 waves)`, done: false })
      vitals.push({ label: 'RIDGE STATION', bar: this.ridge / 100, low: this.ridge < 35 })
    }
    return this.frameFor({
      eyebrow: 'OPERATION MIDNIGHT HAVOC // EPISODE 01', title: 'OPERATION MOUNTAIN SHIELD', phase: st.replace(/^M01_/, '').replace(/_/g, ' '),
      items, vitals, hint,
      abilities: [{ key: 'Q', name: 'RAVEN', status: this.raven.flying ? 'FLYING' : 'READY' }, { key: '—', name: 'AEGIS', status: this.ridgeCaptured ? 'ONLINE' : 'OFFLINE' }],
      bounds: BOUNDS,
      markers: [
        { x: ECHO.x, z: ECHO.z, color: '#7ec8ff', size: 4, shape: 'square' },
        { x: RIDGE.x, z: RIDGE.z, color: this.ridgeCaptured ? '#78ffab' : 'rgba(220,233,227,.5)', size: 4, shape: 'diamond' },
        ...(st === 'M01_DRONE_TUTORIAL' ? [{ x: TUTORIAL_BEACON.x, z: TUTORIAL_BEACON.z, color: '#ffd166', size: 4, shape: 'diamond' as const }] : []),
        ...(this.runner.history.includes('M01_FALCON_DOWN') && this.mission.helicopterCrash.controller.state !== 'EVACUATION_COMPLETE' ? [{ x: CRASH.x, z: CRASH.z, color: '#ff5e63', size: 4, shape: 'square' as const }] : [])
      ]
    })
  }
}

function buildMap(scene: THREE.Scene, setCollision: (c: Collision) => void): void {
  const collision = new Collision(BOUNDS)
  setCollision(collision)
  const solids = new Solids(collision)
  const rand = rng(1701)
  groundPatch(scene, 0, 0, HALF * 2 + 200, HALF * 2 + 200, 0x9a9278, 0)
  groundPatch(scene, ECHO.x, ECHO.z, 70, 50, 0x8d8670)
  groundPatch(scene, RIDGE.x, RIDGE.z, 60, 60, 0x83806c)
  groundPatch(scene, CRASH.x, CRASH.z, 60, 60, 0x6e6a58)
  groundPatch(scene, 0, 120, 14, 280, 0x857c63, 0.03)
  const s = (x: number, z: number, w: number, d: number, h: number, c: number) => solids.add(x, z, w, d, h, c)
  s(0, -HALF - 3, HALF * 2 + 12, 6, 12, PALETTE.rockDark); s(0, HALF + 3, HALF * 2 + 12, 6, 12, PALETTE.rockDark)
  s(-HALF - 3, 0, 6, HALF * 2, 12, PALETTE.rockDark); s(HALF + 3, 0, 6, HALF * 2, 12, PALETTE.rockDark)
  // Checkpoint Echo: sandbag walls with a road gap to the south and open flanks.
  s(-24, ECHO.z - 22, 28, 3, 3, PALETTE.sand); s(24, ECHO.z - 22, 28, 3, 3, PALETTE.sand)
  s(-36, ECHO.z, 3, 30, 3, PALETTE.sand); s(36, ECHO.z, 3, 30, 3, PALETTE.sand)
  s(0, ECHO.z + 8, 10, 8, 5, PALETTE.concrete)
  // Ridge Station: a relay hut and mast on a ridge.
  s(RIDGE.x, RIDGE.z - 6, 12, 10, 6, PALETTE.concrete); s(RIDGE.x + 12, RIDGE.z + 4, 3, 3, 20, PALETTE.steel)
  s(RIDGE.x - 18, RIDGE.z + 10, 14, 3, 3, PALETTE.sand)
  // Crash site boulders.
  s(CRASH.x + 20, CRASH.z - 14, 10, 8, 6, PALETTE.rock); s(CRASH.x - 22, CRASH.z + 16, 8, 10, 5, PALETTE.rock)
  const wreck = new THREE.Group()
  const hull = new THREE.Mesh(new THREE.BoxGeometry(3, 2.4, 9), new THREE.MeshLambertMaterial({ color: 0x2d2f2b })); hull.position.y = 1.2
  const tail = new THREE.Mesh(new THREE.BoxGeometry(0.8, 0.8, 7), new THREE.MeshLambertMaterial({ color: 0x2d2f2b })); tail.position.set(0, 1, 7)
  wreck.add(hull, tail); wreck.position.set(CRASH.x, 0, CRASH.z); wreck.rotation.y = 0.7; scene.add(wreck)
  // Evacuation pad.
  groundPatch(scene, EVAC.x, EVAC.z, 30, 30, 0x6f8a62, 0.04)
  // Rocks across the highlands.
  const clear = (x: number, z: number, r: number) =>
    [ECHO, RIDGE, CRASH, EVAC, START, TUTORIAL_BEACON, ...CREW].every(p => Math.hypot(x - p.x, z - p.z) > 36 + r) && !(Math.abs(x) < 14 + r && z > -20)
  for (let i = 0, tries = 0; i < 90 && tries < 4000; tries++) {
    const w = 6 + rand() * 14, d = 6 + rand() * 14, x = -HALF + 12 + rand() * (HALF * 2 - 24), z = -HALF + 12 + rand() * (HALF * 2 - 24)
    if (!clear(x, z, Math.max(w, d) / 2) || solids.overlaps(x, z, Math.max(w, d) / 2, 4)) continue
    s(x, z, w, d, 3 + rand() * 9, rand() > 0.5 ? PALETTE.rock : PALETTE.rockDark)
    i++
  }
  solids.build(scene)
  scene.background = new THREE.Color(0xc4bfa6)
  scene.fog = new THREE.Fog(0xc4bfa6, 120, 420)
}
