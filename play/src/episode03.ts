import * as THREE from 'three'
import {
  AegisSupportController, BrokenCitadelMission, ContactManager, ProtectedZone,
  type E02Carryover, type Episode03Data, type GardunMapData, type StrongholdData, type SurpriseModifiers, type MissionOutcome
} from '../../src/index.js'
import episodeJson from '../../data/missions/episode-03-broken-citadel.json'
import strongholdJson from '../../data/missions/operation-broken-citadel.json'
import mapJson from '../../data/maps/gardun-city.json'
import { Battle, type BattleOptions, type Hostile } from './engine'
import { buildGardun, CAPTURE_RADIUS, CELLARS, HALF, OBJECTIVES, RIVER, SOUTH_GATE, toData, type BuiltGardun } from './gardun'
import { BASE_CONTROLS, controlsHtml, type HudFrame } from './hud'

export const EPISODE_03 = episodeJson as unknown as Episode03Data
const STRONGHOLD = strongholdJson as unknown as StrongholdData
const MAP = mapJson as unknown as GardunMapData

const E = { captureTime: 6, decayTime: 10, waveEvery: 34, falconCooldown: 40, falconRadius: 16, aegisCooldown: 25, aegisDelay: 2.5 }
const GARRISON: Record<number, number> = { 1: 4, 2: 4, 3: 4, 4: 5, 5: 3 }
const PHASE_NAMES = ['', 'Open the City', 'Break Communications', 'Control Old Gardun', 'Neutralise High Ground', 'Assault the Citadel']
const FORCE = '__restore__'

export interface Episode03Options extends BattleOptions {
  carryover: E02Carryover | undefined
  seed: string
  onResult: (outcome: MissionOutcome) => void
}

export class BrokenCitadelGame extends Battle {
  readonly mission: BrokenCitadelMission
  private readonly contacts = new ContactManager()
  private readonly zones: ProtectedZone[]
  private readonly world: BuiltGardun
  private readonly sand: THREE.Points
  private progress = new Map<string, number>()
  private lastPhase: number | undefined
  private lastUnlocks = new Set<string>()
  private lastSurprise: string | undefined
  private mods: SurpriseModifiers = { droneSensorRangeScale: 1, relayInterference: 0, openedRoutes: [], blockedRoutes: [], visibilityScale: 1 }
  private waveT = E.waveEvery
  private falcon = { cd: 0, heli: null as null | { mesh: THREE.Group; from: THREE.Vector3; to: THREE.Vector3; t: number; dur: number; target: { x: number; z: number }; struck: boolean } }
  private aegisReq = { cd: 0, pending: null as null | { id: string; t: number; marker: THREE.Mesh } }
  private contactT = 0

  constructor(root: HTMLElement, private readonly o: Episode03Options) {
    super(root, o)
    const aegis = new AegisSupportController(this.contacts)
    this.mission = new BrokenCitadelMission(EPISODE_03, STRONGHOLD, MAP, aegis, o.carryover, o.seed)
    this.zones = EPISODE_03.protectedZones.map(z => new ProtectedZone(z.id, z.type, z.position, z.radius))
    this.world = buildGardun(this.scene)
    this.collision = this.world.collision
    Object.assign(this.player, { x: SOUTH_GATE.x, z: SOUTH_GATE.z })

    const sandGeo = new THREE.BufferGeometry()
    const pts = new Float32Array(1500 * 3)
    for (let i = 0; i < pts.length; i += 3) { pts[i] = (Math.random() - 0.5) * 160; pts[i + 1] = Math.random() * 30; pts[i + 2] = (Math.random() - 0.5) * 160 }
    sandGeo.setAttribute('position', new THREE.BufferAttribute(pts, 3))
    this.sand = new THREE.Points(sandGeo, new THREE.PointsMaterial({ color: 0xd9b27a, size: 0.6, transparent: true, opacity: 0.7 }))
    this.sand.visible = false
    this.scene.add(this.sand)

    this.hud.controls = controlsHtml([...BASE_CONTROLS, [['F'], 'Falcon air support at the cursor'], [['G'], 'Aegis on a confirmed hostile'], [['E'], 'Use cellar hatch']])
    this.mission.runner.events.on('mission:checkpoint', ({ checkpoint }) => {
      if (checkpoint.state.startsWith('E03_PHASE')) this.hud.toast('Checkpoint saved', 'good')
    })
    this.mission.beginAssault()
    this.sync()
    this.hud.toast(EPISODE_03.briefing, 'info', 7)
    this.start()
  }

  // ---------- mission sync ----------

  /** Reads the mission after every call into it and updates the world to match. */
  private sync(): void {
    const m = this.mission
    const phase = m.phase
    if (phase !== this.lastPhase) {
      this.lastPhase = phase
      if (phase !== undefined) {
        const p = STRONGHOLD.phases.find(x => x.order === phase)
        this.hud.toast(`Phase ${phase}: ${p?.name ?? ''}`, 'phase', 4)
        this.sound.chime(true)
        this.garrison(phase)
        this.waveT = E.waveEvery
      }
    }
    for (const u of m.unlocks) if (!this.lastUnlocks.has(u)) {
      this.lastUnlocks.add(u)
      const text: Record<string, string> = {
        falcon_air_support: m.airSupportAvailable ? 'Falcon air support available. Press F.' : 'Airfield taken, but no Falcon crew made it through Episode 02.',
        enemy_comms_degraded: 'Enemy communications degraded.',
        aegis_online: 'Aegis online. Press G on a confirmed target.',
        medical_station_protected: 'Medical Station secured. It stays a protected zone.'
      }
      this.hud.toast(text[u] ?? u, 'good', 5)
    }
    const surprise = m.surprise.active
    if (surprise !== this.lastSurprise) {
      const announce = this.lastSurprise !== FORCE
      this.lastSurprise = surprise
      this.applyModifiers(m.surprise.modifiers, surprise, announce)
    }
    if (m.state === 'E03_ASSAULT_FAILED') {
      this.o.onResult('FAILED')
      this.endScreen({
        eyebrow: 'ASSAULT FAILED', eyebrowKind: 'warn', title: 'Reinforcements exhausted', text: m.runner.objective, stats: this.statsBlock(),
        buttons: [{ label: 'Restore checkpoint', primary: true, action: () => this.restoreCheckpoint() }, { label: 'Quit to menu', action: () => this.exit() }]
      })
    }
    if (m.runner.outcome === 'SUCCESS') {
      this.o.onResult('SUCCESS')
      this.endScreen({
        eyebrow: 'OPERATION COMPLETE', eyebrowKind: 'good', title: 'Gardun Citadel secured',
        text: `All five phases taken with ${m.reinforcements} reinforcements to spare.`, stats: this.statsBlock(),
        buttons: [{ label: 'Back to campaign', primary: true, action: () => this.exit() }]
      })
    }
  }

  private applyModifiers(mods: SurpriseModifiers, event: string | undefined, announce: boolean): void {
    this.mods = mods
    this.visibility = mods.visibilityScale
    const night = event === 'nightfall', storm = event === 'sandstorm'
    const fogColor = night ? 0x0e141b : storm ? 0xc49d64 : 0xd8c9a6
    ;(this.scene.background as THREE.Color).setHex(fogColor)
    this.scene.fog = new THREE.Fog(fogColor, 40 * mods.visibilityScale + 20, 420 * mods.visibilityScale)
    this.hemi.intensity = night ? 0.28 : storm ? 0.95 : 1.15
    this.sun.intensity = night ? 0.12 : storm ? 0.9 : 1.6
    this.torch.intensity = night ? 160 : 0
    this.sand.visible = storm
    const b = this.world.bridgeBarrier
    const blocked = mods.blockedRoutes.includes('riverbed_bridge')
    if (blocked && !b.mesh.visible) { this.collision.add(b.box); b.mesh.visible = true }
    if (!blocked && b.mesh.visible) { this.collision.remove(b.box); b.mesh.visible = false }
    const cellars = mods.openedRoutes.includes('old_gardun_cellars')
    this.world.cellarMeshes.forEach(c => { c.visible = cellars })
    const label: Record<string, string> = {
      sandstorm: 'Sandstorm: visibility and drone sensors reduced.',
      communications_outage: 'Communications outage: the tactical map is unreliable.',
      tunnel_discovery: 'Tunnel discovered: the Old Gardun cellars are open. Press E at a hatch.',
      bridge_obstruction: 'The riverbed bridge is blocked. Use the fords.',
      nightfall: 'Nightfall: visibility reduced.'
    }
    if (!announce) return
    if (event) { this.hud.toast(`Surprise event. ${label[event] ?? event}`, 'warn', 6); this.sound.chime(false) }
    else this.hud.toast('Conditions back to normal.', 'info', 3)
  }

  // ---------- hostiles ----------

  private garrison(phase: number): void {
    // Stragglers from earlier phases far from the player stand down.
    for (const e of this.enemies) if (e.alive && Math.hypot(e.x - this.player.x, e.z - this.player.z) > 70) this.dropHostile(e)
    for (const id of this.mission.openObjectives()) {
      const o = OBJECTIVES[id]; if (!o) continue
      for (let i = 0; i < (GARRISON[phase] ?? 4); i++) {
        const a = Math.random() * Math.PI * 2, r = 6 + Math.random() * 18
        this.spawnHostile(o.x + Math.cos(a) * r, o.z + Math.sin(a) * r, { anchor: { x: o.x, z: o.z } })
      }
    }
    // Survivors re-anchor to the nearest open objective.
    for (const e of this.enemies) if (e.alive) e.anchor = this.nearestOpen(e.x, e.z) ?? e.anchor
  }

  private wave(): void {
    const target = this.nearestOpen(this.player.x, this.player.z)
    if (!target) return
    const size = Math.max(1, Math.round(3 * this.mission.counterattackStrength))
    const a = Math.atan2(target.z - this.player.z, target.x - this.player.x)
    for (let i = 0; i < size; i++) {
      const ang = a + (Math.random() - 0.5) * 0.8
      this.spawnHostile(target.x + Math.cos(ang) * 60, target.z + Math.sin(ang) * 60, { anchor: target })
    }
    this.hud.toast('Enemy counterattack inbound', 'warn', 2.5)
  }

  private nearestOpen(x: number, z: number): { x: number; z: number } | undefined {
    let best: { x: number; z: number } | undefined, bd = Infinity
    for (const id of this.mission.openObjectives()) {
      const o = OBJECTIVES[id]; if (!o) continue
      const d = Math.hypot(o.x - x, o.z - z)
      if (d < bd) { bd = d; best = { x: o.x, z: o.z } }
    }
    return best
  }

  private dropHostile(e: Hostile): void {
    this.removeHostile(e)
    this.contacts.loseTracking(e.id, this.time * 1000)
  }

  protected override onHostileDown(e: Hostile): void { this.contacts.loseTracking(e.id, this.time * 1000) }
  protected override hostileFireRate(): number { return this.mission.counterattackStrength }

  /** The player's observations feed the contact picture Aegis validates against. */
  private updateContacts(dt: number): void {
    if ((this.contactT -= dt) > 0) return
    this.contactT = 0.2
    const p = this.player
    for (const e of this.enemies) {
      if (!e.alive) continue
      const dist = Math.hypot(e.x - p.x, e.z - p.z)
      const observed = p.alive && dist < 75 * this.mods.visibilityScale + 10 && this.collision.clear(p.x, p.z, e.x, e.z)
      if (observed) {
        this.contacts.upsert({ id: e.id, classification: 'GAME_UNIT', position: toData(e.x, e.z), source: 'PLAYER', time: this.time * 1000, quality: 0.9 })
        e.seenFor += 0.2
        if (e.seenFor >= 1) this.contacts.confirm(e.id)
      } else if (this.contacts.get(e.id)?.currentlyObserved) { this.contacts.loseTracking(e.id, this.time * 1000); e.seenFor = 0 }
    }
  }

  // ---------- player ----------

  protected override onPlayerDown(): void {
    this.mission.spendReinforcements(1)
    this.sync()
  }

  /** Forward spawn at the most recently captured objective, else South Gate. */
  protected respawnPoint(): { x: number; z: number } {
    const last = this.mission.capturedObjectives.at(-1)
    const o = last ? OBJECTIVES[last] : undefined
    return o ? { x: o.x, z: o.z + 16 } : { ...SOUTH_GATE }
  }

  protected override onKey(k: string): void {
    if (k === 'f') this.callFalcon()
    if (k === 'g') this.callAegis()
    if (k === 'e') this.useCellar()
  }

  // ---------- objectives ----------

  private updateObjectives(dt: number): void {
    const open = new Set(this.mission.openObjectives())
    const captured = new Set(this.mission.capturedObjectives)
    const p = this.player
    for (const [id, m] of this.world.objectiveMeshes) {
      const o = OBJECTIVES[id]!
      const ringMat = m.ring.material as THREE.MeshBasicMaterial
      const beaconMat = m.beacon.material as THREE.MeshBasicMaterial
      if (captured.has(id)) {
        ringMat.color.setHex(0x78ffab); ringMat.opacity = 0.7; m.beacon.visible = false
        m.fill.scale.setScalar(1); (m.fill.material as THREE.MeshBasicMaterial).color.setHex(0x78ffab)
        continue
      }
      if (!open.has(id)) { ringMat.color.setHex(0x6d6a63); ringMat.opacity = 0.35; m.beacon.visible = false; m.fill.scale.setScalar(0.001); continue }
      const inPlayer = p.alive && Math.hypot(p.x - o.x, p.z - o.z) < CAPTURE_RADIUS
      const inEnemy = this.enemies.some(e => e.alive && Math.hypot(e.x - o.x, e.z - o.z) < CAPTURE_RADIUS)
      let v = this.progress.get(id) ?? 0
      if (inPlayer && !inEnemy) v += dt / E.captureTime
      else if (!inPlayer && inEnemy) v -= dt / E.decayTime
      v = Math.max(0, Math.min(1, v))
      this.progress.set(id, v)
      const contested = inPlayer && inEnemy
      ringMat.color.setHex(contested ? 0xff5e63 : 0xffd166); ringMat.opacity = 0.75 + Math.sin(this.time * 4) * 0.2
      m.beacon.visible = true; beaconMat.color.setHex(contested ? 0xff5e63 : 0xffd166)
      m.fill.scale.setScalar(Math.max(0.001, v))
      ;(m.fill.material as THREE.MeshBasicMaterial).color.setHex(0x78ffab)
      if (v >= 1 && this.mission.captureObjective(id)) {
        this.progress.delete(id)
        this.hud.toast(`${o.label} secured`, 'good', 3)
        this.sound.chime(true)
        this.sync()
      }
    }
  }

  // ---------- support ----------

  private callFalcon(): void {
    const m = this.mission
    if (!m.unlocks.includes('falcon_air_support')) return this.hud.toast('Falcon air support: take the Airfield first.', 'warn', 3)
    if (!m.airSupportAvailable) return this.hud.toast('Falcon air support unavailable: no crew came through Episode 02.', 'warn', 3)
    if (this.falcon.cd > 0 || this.falcon.heli) return this.hud.toast(`Falcon rearming: ${Math.ceil(this.falcon.cd)}s`, 'info', 2)
    const target = { x: this.mouse.ground.x, z: this.mouse.ground.z }
    if (this.zones.some(z => z.contains(toData(target.x, target.z)))) return this.hud.toast('Refused: that point is inside the Medical Station protected zone.', 'warn', 3.5)
    const mesh = helicopterMesh()
    const from = new THREE.Vector3(target.x, 26, target.z + 240), to = new THREE.Vector3(target.x, 26, target.z - 240)
    mesh.position.copy(from)
    this.scene.add(mesh)
    this.falcon.heli = { mesh, from, to, t: 0, dur: 4.4, target, struck: false }
    this.hud.toast('Falcon inbound', 'good', 2)
  }

  private updateFalcon(dt: number): void {
    this.falcon.cd = Math.max(0, this.falcon.cd - dt)
    const h = this.falcon.heli
    if (!h) return
    h.t += dt
    const k = h.t / h.dur
    h.mesh.position.lerpVectors(h.from, h.to, k)
    h.mesh.getObjectByName('rotor')!.rotation.y += dt * 30
    if (!h.struck && k >= 0.5) {
      h.struck = true
      for (let i = 0; i < 10; i++) this.puff(h.target.x + (Math.random() - 0.5) * E.falconRadius * 1.6, h.target.z + (Math.random() - 0.5) * E.falconRadius * 1.6, 0x4a4036, 4)
      for (const e of this.enemies) if (e.alive && Math.hypot(e.x - h.target.x, e.z - h.target.z) < E.falconRadius) this.killHostile(e)
      this.sound.boom()
    }
    if (k >= 1) { this.scene.remove(h.mesh); this.falcon.heli = null; this.falcon.cd = E.falconCooldown }
  }

  private callAegis(): void {
    const aegis = this.mission.aegis
    if (aegis.state === 'OFFLINE') return this.hud.toast('Aegis offline: take the Mountain Fort.', 'warn', 3)
    if (this.aegisReq.pending) return
    if (this.aegisReq.cd > 0) return this.hud.toast(`Aegis recharging: ${Math.ceil(this.aegisReq.cd)}s`, 'info', 2)
    let target: Hostile | undefined, bd = 10
    for (const e of this.enemies) {
      if (!e.alive) continue
      const d = Math.hypot(e.x - this.mouse.ground.x, e.z - this.mouse.ground.z)
      if (d < bd) { bd = d; target = e }
    }
    if (!target) return this.hud.toast('Aegis: no hostile under the cursor.', 'info', 2)
    const reasons: Record<string, string> = {
      NO_TARGET: 'no contact on that hostile yet. Keep it in sight.',
      TARGET_NOT_CONFIRMED: 'target not confirmed. Keep it in sight for a moment.',
      TRACKING_REQUIRED: 'target not currently observed.',
      PROTECTED_ZONE: 'refused. The target is inside the Medical Station protected zone.',
      SUPPORT_UNAVAILABLE: 'support unavailable.'
    }
    let r = aegis.selectTarget(target.id)
    if (r.valid) r = aegis.validateSelection()
    if (!r.valid) return this.hud.toast(`Aegis: ${reasons[r.reason] ?? r.reason}`, 'warn', 3)
    aegis.authorize(Math.round(this.time * 1000))
    const marker = new THREE.Mesh(new THREE.RingGeometry(2.2, 2.8, 32), new THREE.MeshBasicMaterial({ color: 0x8fd3ff, transparent: true, opacity: 0.9, side: THREE.DoubleSide }))
    marker.rotation.x = -Math.PI / 2
    this.scene.add(marker)
    this.aegisReq.pending = { id: target.id, t: E.aegisDelay, marker }
    this.hud.toast('Aegis authorised', 'good', 2)
  }

  private updateAegis(dt: number): void {
    this.aegisReq.cd = Math.max(0, this.aegisReq.cd - dt)
    const req = this.aegisReq.pending
    if (!req) return
    const e = this.enemies.find(x => x.id === req.id)
    if (e) { req.marker.position.set(e.x, 0.2, e.z); req.marker.scale.setScalar(1 + Math.sin(this.time * 12) * 0.15) }
    req.t -= dt
    if (req.t > 0) return
    this.scene.remove(req.marker)
    this.aegisReq.pending = null
    const aegis = this.mission.aegis
    const c = this.contacts.get(req.id)
    const stillValid = e?.alive && c?.currentlyObserved && !this.zones.some(z => z.contains(toData(e.x, e.z)))
    if (!stillValid || !e) { aegis.abort(); this.hud.toast('Aegis aborted: target lost.', 'warn', 2.5); return }
    aegis.commit()
    aegis.complete(Math.round(this.time * 1000))
    const beam = new THREE.Mesh(new THREE.CylinderGeometry(1.4, 1.4, 60, 12, 1, true), new THREE.MeshBasicMaterial({ color: 0x8fd3ff, transparent: true, opacity: 0.7 }))
    beam.position.set(e.x, 30, e.z)
    this.flash(beam, 0.6)
    this.killHostile(e)
    this.sound.boom()
    this.aegisReq.cd = E.aegisCooldown
  }

  private useCellar(): void {
    if (!this.mods.openedRoutes.includes('old_gardun_cellars')) return
    const p = this.player
    for (const [a, b] of [[CELLARS.a, CELLARS.b], [CELLARS.b, CELLARS.a]] as const) {
      if (Math.hypot(p.x - a.x, p.z - a.z) < 6) {
        p.x = b.x; p.z = b.z + 5
        this.hud.toast('Through the cellars', 'info', 1.5)
        return
      }
    }
  }

  // ---------- checkpoint ----------

  protected restoreCheckpoint(): void {
    if (!this.mission.runner.restoreCheckpoint()) return
    this.clearHostiles()
    this.clearBullets()
    if (this.aegisReq.pending) { this.scene.remove(this.aegisReq.pending.marker); this.aegisReq.pending = null }
    if (this.falcon.heli) { this.scene.remove(this.falcon.heli.mesh); this.falcon.heli = null }
    this.progress.clear()
    this.lastPhase = undefined
    this.lastUnlocks = new Set(this.mission.unlocks)
    this.lastSurprise = FORCE
    this.resume()
    this.sync()
    this.respawn()
    this.hud.toast('Restored from checkpoint', 'good', 2.5)
  }

  // ---------- frame ----------

  protected update(dt: number): void {
    this.updateContacts(dt)
    this.updateObjectives(dt)
    this.updateFalcon(dt)
    this.updateAegis(dt)
    if ((this.waveT -= dt) <= 0) { this.waveT = E.waveEvery; if (this.mission.phase !== undefined) this.wave() }
    if (this.sand.visible) { this.sand.position.set(this.player.x, 0, this.player.z); this.sand.rotation.y += dt * 0.6 }
  }

  protected override pauseCard() {
    const card = super.pauseCard()
    return { ...card, title: 'Operation Broken Citadel', text: `Phase ${this.mission.phase ?? '-'} of 5 · ${this.mission.reinforcements} reinforcements left` }
  }

  protected hudFrame(): HudFrame {
    const m = this.mission, p = this.player
    const aegis = m.aegis.state
    const phase = m.phase
    const anyOne = phase !== undefined && EPISODE_03.phaseRules[String(phase)] === 'any'
    return {
      eyebrow: 'PROJECT PAKTIA // EPISODE 03', title: 'OPERATION BROKEN CITADEL',
      phase: phase ? `PHASE ${phase}/5 · ${PHASE_NAMES[phase]?.toUpperCase() ?? ''}` : '',
      objective: m.runner.objective + (anyOne ? ' (either one)' : ''),
      items: (STRONGHOLD.phases.find(x => x.order === phase)?.objectives ?? []).map(id => ({
        label: OBJECTIVES[id]?.label ?? id, done: m.capturedObjectives.includes(id), progress: this.progress.get(id) ?? 0
      })),
      vitals: [{ label: 'REINFORCEMENTS', value: String(m.reinforcements), low: m.reinforcements <= 5 }],
      hp: Math.max(0, p.hp) / 100, mag: p.mag, magSize: this.mag, reloading: p.reload > 0, auto: p.auto,
      abilities: [
        { key: 'F', name: 'FALCON', status: !m.unlocks.includes('falcon_air_support') ? 'LOCKED' : !m.airSupportAvailable ? 'NO CREW' : this.falcon.heli ? 'INBOUND' : this.falcon.cd > 0 ? `${Math.ceil(this.falcon.cd)}s` : 'READY' },
        { key: 'G', name: 'AEGIS', status: aegis === 'OFFLINE' ? 'OFFLINE' : this.aegisReq.pending ? 'AUTHORISED' : this.aegisReq.cd > 0 ? `${Math.ceil(this.aegisReq.cd)}s` : 'READY' }
      ],
      status: m.surprise.active ? m.surprise.active.replace(/_/g, ' ').toUpperCase() : null,
      map: {
        bounds: { x0: -HALF, z0: -HALF, x1: HALF, z1: HALF },
        lines: [
          { pts: [{ x: -HALF, z: (RIVER.z0 + RIVER.z1) / 2 }, { x: HALF, z: (RIVER.z0 + RIVER.z1) / 2 }], color: 'rgba(160,130,80,.45)', width: RIVER.z1 - RIVER.z0 },
          { pts: [{ x: -72, z: -160 }, { x: -72, z: -301 }, { x: 72, z: -301 }, { x: 72, z: -160 }, { x: -72, z: -160 }], color: 'rgba(200,180,140,.35)' },
          ...(this.mods.blockedRoutes.includes('riverbed_bridge') ? [{ pts: [{ x: -12, z: 109 }, { x: 12, z: 109 }], color: '#ff5e63', width: 3 }] : []),
          ...(this.mods.openedRoutes.includes('old_gardun_cellars') ? [{ pts: [CELLARS.a, CELLARS.b], color: '#ffd166', dash: true }] : [])
        ],
        markers: Object.entries(OBJECTIVES).map(([id, o]) => {
          const state = m.capturedObjectives.includes(id) ? 'done' : m.openObjectives().includes(id) ? 'open' : 'locked'
          return { x: o.x, z: o.z, color: state === 'done' ? '#78ffab' : state === 'open' ? '#ffd166' : 'rgba(220,233,227,.25)', size: state === 'open' ? 4 : 3 }
        }),
        player: { x: p.x, z: p.z, angle: p.angle },
        enemies: this.enemies.filter(e => e.alive).map(e => ({ x: e.x, z: e.z, confirmed: this.contacts.get(e.id)?.state === 'CONFIRMED' })),
        reveal: 120 * this.mods.droneSensorRangeScale,
        interference: this.mods.relayInterference
      }
    }
  }
}

export function helicopterMesh(): THREE.Group {
  const mesh = new THREE.Group()
  const body = new THREE.Mesh(new THREE.BoxGeometry(3, 2.4, 9), new THREE.MeshLambertMaterial({ color: 0x3f4a3c }))
  const tail = new THREE.Mesh(new THREE.BoxGeometry(0.8, 0.8, 8), new THREE.MeshLambertMaterial({ color: 0x3f4a3c })); tail.position.z = 8
  const rotor = new THREE.Mesh(new THREE.BoxGeometry(18, 0.2, 0.8), new THREE.MeshLambertMaterial({ color: 0x1a1a1a })); rotor.position.y = 1.6; rotor.name = 'rotor'
  mesh.add(body, tail, rotor)
  return mesh
}
