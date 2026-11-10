import * as THREE from 'three'
import {
  AegisSupportController, ContactManager, E04_PROTECTED, OpenStraitMission, parseMissionDefinition,
  type Episode04Rules, type LaneId, type MissionOutcome
} from '../../src/index.js'
import episodeJson from '../../data/missions/episode-04-open-strait.json'
import mapJson from '../../data/maps/kharg-hormuz.json'
import { Collision, PALETTE, Solids, groundPatch, rng } from './collision'
import type { Friendly } from './engine'
import { BASE_CONTROLS, controlsHtml, type HudFrame } from './hud'
import { ScenarioBattle, type BattleOptions } from './scenario'

// Episode 04, Operation Open Strait. A hypothetical scenario on real
// geography (setting real_world_hypothetical): the participants are the US
// Navy and Marines against Iranian forces, and nothing here depicts real
// people, real weapons systems or real targeting. Hazards are abstract
// obstacles, the kit is the fictional KR-47 / Raven / Falcon, and every
// rating is a 1-10 game-balance value. The state graph is
// data/missions/episode-04-open-strait.json; OpenStraitMission owns the
// island assault, the terminal, the lanes and the tankers.

const RAW = episodeJson as unknown as Record<string, unknown> & Episode04Rules
const DEF = parseMissionDefinition(RAW, (mapJson as { sectors: { id: string }[] }).sectors.map(s => s.id))
export const HALF = 312
const BOUNDS = { x0: -HALF, z0: -HALF, x1: HALF, z1: HALF }
const METRES = 4
// World units back to the game-space metres E04_PROTECTED uses.
const toMetres = (x: number, z: number) => ({ x: 1250 + x * METRES, z: 1250 + z * METRES })

const SHORE_Z = 182
const SEA_START = { x: 0, z: 165 }
const BEACHES = { north: { x: 120, z: -286 }, west: { x: -286, z: 30 } }
const AIRSTRIP = { x: -130, z: 105 }
const TERMINAL = { x: 232, z: -92 }
const TOWN = { x: 62, z: 87 }
const LANE_Z: Record<LaneId, number> = { inbound: 205, outbound: 226 }
const HAZARDS: Record<LaneId, number[]> = { inbound: [-200, -80, 40, 160], outbound: [-150, 0, 150] }
const FACILITIES = [
  { id: 'east_jetty', name: 'East jetty', x: 292, z: -30, w: 10, d: 38 },
  { id: 'sea_island_jetty', name: 'Sea Island jetty', x: 292, z: -140, w: 10, d: 34 },
  { id: 'tank_farm', name: 'Tank farm', x: 212, z: -102, w: 44, d: 30 },
  { id: 'pump_station', name: 'Pump station', x: 246, z: -62, w: 16, d: 16 }
]
const E = { tankerSpeed: 9, tankerEvery: 14, clearTime: 6, strayDamage: 0.04, falconCd: 25, falconRadius: 18, raidEvery: 18 }

interface Hazard { lane: LaneId; x: number; revealed: boolean; cleared: boolean; t: number; mesh: THREE.Mesh }
interface Tanker { id: string; lane: LaneId; s: number; mesh: THREE.Group; friendly: Friendly; hp: number }

export interface Episode04Options extends BattleOptions {
  /** Outcome of Episode 03, for the reinforcement bonus. */
  episode3: MissionOutcome | undefined
  onResult: (outcome: MissionOutcome, summary: { tankersThrough: number; terminalIntegrity: number }) => void
}

export class OpenStraitGame extends ScenarioBattle {
  protected runner!: OpenStraitMission['runner']
  private mission!: OpenStraitMission
  private readonly o: Episode04Options
  private respawnAt = { ...SEA_START }
  private scouted = new Set<'north' | 'west'>()
  private hazards: Hazard[] = []
  private tankers: Tanker[] = []
  private launched = 0
  private tankerT = 0
  private raidT = 0
  private falconCd = 0
  private falconPending: { t: number; x: number; z: number } | null = null
  private warnT = 0
  private ended = false
  private readonly facilityMeshes = new Map<string, THREE.Mesh>()
  private readonly lanePts: Record<LaneId, { x: number; z: number }[]> = {
    inbound: [{ x: -HALF + 6, z: LANE_Z.inbound }, { x: HALF - 6, z: LANE_Z.inbound }],
    outbound: [{ x: HALF - 6, z: LANE_Z.outbound }, { x: -HALF + 6, z: LANE_Z.outbound }]
  }

  constructor(root: HTMLElement, o: Episode04Options) {
    super(root, o)
    this.o = o
    this.collision = buildMap(this.scene, this.facilityMeshes)
    Object.assign(this.player, { x: SEA_START.x, z: SEA_START.z })
    this.initRaven()
    this.raven.x = SEA_START.x; this.raven.z = SEA_START.z
    this.hud.controls = controlsHtml([...BASE_CONTROLS, [['Q'], 'Fly Raven / back to the squad'], [['F'], 'Falcon cover at the cursor (after the airstrip)'], [['1', '2'], 'Pick the landing beach']])
    this.newMission()
    this.runner.events.on('mission:state', ({ to }) => this.onState(to))
    this.runner.events.on('mission:complete', ({ outcome }) => this.finish(outcome))
    this.start()
    this.briefing({
      eyebrow: 'OPERATION MIDNIGHT HAVOC // EPISODE 04 // HYPOTHETICAL SCENARIO', title: 'Operation Open Strait',
      text: 'An invented future scenario on real geography: the Strait of Hormuz is closed to shipping. Marines take Kharg Island with its oil terminal intact, clear the lanes and escort the tankers through. Fictional kit, game-balance ratings only.',
      button: 'Begin'
    }, () => this.onState(this.runner.state))
  }

  // ---------- mission ----------

  private newMission(): void {
    const contacts = new ContactManager()
    this.mission = new OpenStraitMission(DEF, RAW, new AegisSupportController(contacts), contacts, this.o.episode3)
    this.runner = this.mission.runner
  }

  private onState(state: string): void {
    this.clearSites()
    const say = (t: string) => this.hud.toast(t, 'phase', 4.5)
    switch (state) {
      case 'E04_BEACH_RECON':
        this.scouted.clear()
        this.clearHostiles()
        this.group(8, BEACHES.north.x, BEACHES.north.z + 12, 20, BEACHES.north, 40, 'beach-north')
        this.group(3, BEACHES.west.x + 12, BEACHES.west.z, 18, BEACHES.west, 40, 'beach-west')
        say('Press Q and fly Raven over both landing beaches to mark their defences.')
        break
      case 'E04_CHOOSE_BEACH': {
        const n = this.alive('beach-north').length, w = this.alive('beach-west').length
        this.choose('DECISION · LANDING BEACH', 'Choose the landing beach', this.runner.objective, [
          { label: 'North beach', detail: `${n} defenders marked, so ${n} reinforcements lost on landing. Close to the oil terminal: terminal first, airstrip second.`, pick: () => this.mission.chooseBeach('north_beach') },
          { label: 'West beach', detail: `${w} defenders marked, so ${w} reinforcements lost on landing. Quieter, and the airstrip comes first for air cover.`, pick: () => this.mission.chooseBeach('west_beach') }
        ])
        break
      }
      case 'E04_NORTH_LANDING': this.land('north'); break
      case 'E04_WEST_LANDING': this.land('west'); break
      case 'E04_TERMINAL_FIRST': case 'E04_TERMINAL_SECOND':
        this.respawnAt = { x: TERMINAL.x - 60, z: TERMINAL.z + 20 }
        this.group(6, TERMINAL.x, TERMINAL.z, 30, TERMINAL, 55, 'terminal')
        this.addSite({ id: 'terminal', x: TERMINAL.x, z: TERMINAL.z, r: 30, label: 'Oil terminal', seconds: 8 })
        say('Secure the terminal without hitting the jetties, tanks or pumps. Aim clear of them.')
        break
      case 'E04_AIRSTRIP_FIRST': case 'E04_AIRSTRIP_SECOND':
        this.respawnAt = { x: AIRSTRIP.x - 40, z: AIRSTRIP.z - 20 }
        this.group(5, AIRSTRIP.x, AIRSTRIP.z, 26, AIRSTRIP, 50, 'airstrip')
        this.addSite({ id: 'airstrip', x: AIRSTRIP.x, z: AIRSTRIP.z, r: 24, label: 'Airstrip', seconds: 8 })
        say('Take the airstrip so helicopters can operate from the island.')
        break
      case 'E04_ISLAND_SECURED':
        this.clearHostiles()
        this.respawnAt = { x: AIRSTRIP.x + 60, z: SHORE_Z - 20 }
        say('Kharg Island secured.')
        break
      case 'E04_LANE_CLEARANCE':
        this.respawnAt = { x: 0, z: SHORE_Z - 14 }
        this.makeHazards()
        say('Fly Raven (Q) along both lanes to find the hazards. Clearance teams remove each one Raven marks.')
        break
      case 'E04_TANKER_ESCORT':
        this.clearHazardMeshes()
        this.clearTankers()
        this.launched = 0; this.tankerT = 5; this.raidT = 4
        this.raven.flying = false
        say('Escort the tankers. Hold the south shore: raiders will fire on the lanes.')
        for (const x of [-190, -65, 65, 190]) this.group(3, x, SHORE_Z - 12, 10, { x, z: SHORE_Z - 12 }, 30, 'raider')
        break
      case 'E04_DEBRIEF':
        this.mission.finish()
        break
    }
  }

  private land(beach: 'north' | 'west'): void {
    const other = beach === 'north' ? 'beach-west' : 'beach-north'
    for (const e of this.alive(other)) this.removeHostile(e)
    const at = BEACHES[beach]
    this.respawnAt = { ...at }
    Object.assign(this.player, { x: at.x, z: at.z })
    this.raven.flying = false
    this.addSite({ id: 'beachhead', x: at.x, z: at.z + (beach === 'north' ? 14 : 0), r: 26, label: 'Beachhead', seconds: 7 })
    this.hud.toast(`Marines land on the ${beach} beach. Clear the defenders and hold the ring.`, 'phase', 4.5)
  }

  protected onSite(id: string): void {
    if (id === 'beachhead') this.mission.secureBeachhead()
    else if (id === 'terminal') this.mission.secureTerminal()
    else if (id === 'airstrip') {
      this.mission.secureAirstrip()
      this.hud.toast('Airstrip taken. Falcon cover available: press F.', 'good', 4)
    }
  }

  private finish(outcome: MissionOutcome): void {
    if (this.ended) return
    const r = this.mission.result()
    if (outcome === 'FAILED') {
      this.o.onResult('FAILED', { tankersThrough: r.tankersThrough, terminalIntegrity: r.terminalIntegrity })
      this.failCard('The operation failed', this.runner.objective)
      return
    }
    this.ended = true
    this.o.onResult(outcome, { tankersThrough: r.tankersThrough, terminalIntegrity: r.terminalIntegrity })
    this.winCard({
      eyebrow: 'OPERATION COMPLETE', title: outcome === 'SUCCESS' ? 'The strait is open' : 'The strait is open, supply only partly restored',
      text: this.runner.objective, good: outcome === 'SUCCESS',
      stats: [
        { label: 'tankers through', value: `${r.tankersThrough}/${this.mission.escort.tankers.length}` },
        { label: 'terminal integrity', value: `${Math.round(r.terminalIntegrity * 100)}%` },
        { label: 'reinforcements left', value: String(this.mission.reinforcements) }
      ]
    })
  }

  protected respawnPoint(): { x: number; z: number } { return { ...this.respawnAt } }

  protected override onPlayerDown(): void {
    if (this.runner.complete) return
    this.mission.spendReinforcements(1)
    if (this.mission.reinforcements <= 5 && this.mission.reinforcements > 0) this.hud.toast(`${this.mission.reinforcements} reinforcements left`, 'warn', 2.5)
  }

  protected restoreCheckpoint(): void {
    if (!this.runner.checkpoint) return
    this.clearHostiles(); this.clearBullets(); this.clearSites(); this.clearHazardMeshes(); this.clearTankers()
    this.raven.flying = false
    this.falconPending = null
    this.resume()
    this.ended = false
    this.runner.restoreCheckpoint()
    this.restoreFacilityLooks()
    this.respawn()
    this.hud.toast('Restored from checkpoint', 'good', 2.5)
  }

  // ---------- keys ----------

  protected override onKey(key: string): void {
    super.onKey(key)
    if (key === 'f') this.callFalcon()
  }

  private callFalcon(): void {
    if (!this.mission.airSupport) return void this.hud.toast('No air cover yet: take the airstrip first.', 'info', 2.5)
    if (this.falconCd > 0 || this.falconPending) return void this.hud.toast('Falcon is rearming.', 'info', 2)
    const g = this.mouse.ground
    const m = toMetres(g.x, g.z)
    const blocked = E04_PROTECTED.find(z => Math.hypot(z.position.x - m.x, z.position.z - m.z) < z.radius + E.falconRadius * METRES)
    if (blocked) return void this.hud.toast(`Refused: ${blocked.id.replace(/_/g, ' ')} is a protected zone.`, 'warn', 3)
    this.falconPending = { t: 1.6, x: g.x, z: g.z }
    this.hud.toast('Falcon cover inbound.', 'info', 2)
  }

  // ---------- frame ----------

  protected update(dt: number): void {
    const st = this.runner.state
    this.updateRaven(dt, BOUNDS)
    this.updateSites(dt)
    this.falconCd = Math.max(0, this.falconCd - dt)
    this.warnT = Math.max(0, this.warnT - dt)
    this.updateFalcon(dt)
    this.updateStrayFire()

    if (st === 'E04_BEACH_RECON' && this.raven.flying) {
      for (const b of ['north', 'west'] as const) {
        if (!this.scouted.has(b) && Math.hypot(this.raven.x - BEACHES[b].x, this.raven.z - BEACHES[b].z) < 45) {
          this.scouted.add(b)
          this.hud.toast(`${b === 'north' ? 'North' : 'West'} beach marked: ${this.alive(`beach-${b}`).length} defenders.`, 'good', 3)
        }
      }
      if (this.scouted.size === 2) {
        this.mission.scoutBeaches({ north: this.alive('beach-north').length, west: this.alive('beach-west').length })
        this.raven.flying = false
      }
    } else if (st === 'E04_LANE_CLEARANCE') this.updateHazards(dt)
    else if (st === 'E04_TANKER_ESCORT') this.updateEscort(dt)

    this.autoAdvance()
  }

  // ---------- lanes ----------

  private makeHazards(): void {
    this.clearHazardMeshes()
    for (const lane of Object.keys(HAZARDS) as LaneId[]) for (const x of HAZARDS[lane]) {
      const mesh = new THREE.Mesh(new THREE.OctahedronGeometry(2.4), new THREE.MeshBasicMaterial({ color: 0xff9b3d }))
      mesh.position.set(x, 1.2, LANE_Z[lane]); mesh.visible = false
      this.scene.add(mesh)
      this.hazards.push({ lane, x, revealed: false, cleared: false, t: 0, mesh })
    }
  }

  private clearHazardMeshes(): void {
    for (const h of this.hazards) this.scene.remove(h.mesh)
    this.hazards = []
  }

  private updateHazards(dt: number): void {
    for (const h of this.hazards) {
      if (h.cleared) continue
      if (!h.revealed) {
        if (this.raven.flying && Math.hypot(this.raven.x - h.x, this.raven.z - LANE_Z[h.lane]) < 30) {
          h.revealed = true; h.mesh.visible = true
          this.mission.revealHazards(h.lane, 1)
          this.hud.toast(`${h.lane} lane: hazard marked`, 'info', 2)
        }
        continue
      }
      h.t += dt
      h.mesh.rotation.y += dt * 2
      if (h.t >= E.clearTime && this.mission.clearHazard(h.lane)) {
        h.cleared = true
        this.puff(h.x, LANE_Z[h.lane], 0xffd9a0, 5)
        this.scene.remove(h.mesh)
        this.hud.toast(`${h.lane} lane: hazard cleared`, 'good', 2)
      }
    }
  }

  // ---------- tankers ----------

  private updateEscort(dt: number): void {
    const m = this.mission
    // Launch one tanker at a time, alternating lanes.
    if (this.launched < m.escort.tankers.length && (this.tankerT -= dt) <= 0) {
      const t = m.escort.tankers[this.launched]!
      const lane: LaneId = this.launched % 2 === 0 ? 'inbound' : 'outbound'
      if (m.startTransit(t.id, lane)) this.spawnTanker(t.id, lane)
      this.launched++
      this.tankerT = E.tankerEvery
    }
    for (const t of [...this.tankers]) {
      t.s += E.tankerSpeed * dt
      const pts = this.lanePts[t.lane], len = Math.abs(pts[1]!.x - pts[0]!.x), k = Math.min(1, t.s / len)
      const x = pts[0]!.x + (pts[1]!.x - pts[0]!.x) * k
      t.mesh.position.set(x, 0, LANE_Z[t.lane])
      if (t.hp <= 0) { this.dropTanker(t); continue }
      if (k >= 1) { this.dropTanker(t); m.tankerArrived(t.id); this.hud.toast(`${t.id} is through`, 'good', 2.5) }
    }
    // Raiders keep coming at the shore while tankers are on the lanes.
    if (this.tankers.length > 0 && (this.raidT -= dt) <= 0) {
      this.raidT = E.raidEvery
      const x = -220 + Math.random() * 440
      this.group(3, x, SHORE_Z - 10, 8, { x, z: SHORE_Z - 12 }, 28, 'raider')
      this.hud.toast('More raiders on the south shore', 'warn', 2.5)
    }
  }

  private spawnTanker(id: string, lane: LaneId): void {
    const mesh = new THREE.Group()
    const hull = new THREE.Mesh(new THREE.BoxGeometry(24, 4, 6), new THREE.MeshLambertMaterial({ color: 0x4a5560 })); hull.position.y = 2
    const house = new THREE.Mesh(new THREE.BoxGeometry(5, 5, 5), new THREE.MeshLambertMaterial({ color: 0xcfd3d6 })); house.position.set(lane === 'inbound' ? -8 : 8, 6, 0)
    mesh.add(hull, house)
    if (lane === 'outbound') mesh.rotation.y = Math.PI
    this.scene.add(mesh)
    const self = this
    const tanker: Tanker = {
      id, lane, s: 0, mesh, hp: 1,
      friendly: {
        id, r: 6, get x() { return mesh.position.x }, get z() { return mesh.position.z }, get alive() { return tanker.hp > 0 },
        hit: (d: number) => { tanker.hp = Math.max(0, tanker.hp - d * 0.012); self.mission.damageTanker(id, d * 0.012) }
      }
    }
    this.tankers.push(tanker)
    this.hud.toast(`${id} is under way (${lane} lane)`, 'info', 2.5)
  }

  private dropTanker(t: Tanker): void {
    this.scene.remove(t.mesh)
    this.tankers = this.tankers.filter(x => x !== t)
    if (t.hp <= 0) { this.puff(t.mesh.position.x, t.mesh.position.z, 0x222222, 8); this.hud.toast(`${t.id} was lost`, 'warn', 3) }
  }

  private clearTankers(): void {
    for (const t of this.tankers) this.scene.remove(t.mesh)
    this.tankers = []
  }

  protected override friendlies(): Friendly[] { return [this.playerFriendly, ...this.tankers.map(t => t.friendly)] }

  // ---------- terminal ----------

  /** Rounds that land in a terminal facility damage it; the mission rewards taking the terminal intact. */
  private updateStrayFire(): void {
    for (const b of this.bullets) {
      if (!b.friendly || b.life <= 0) continue
      for (const f of FACILITIES) {
        if (Math.abs(b.x - f.x) < f.w / 2 + 1 && Math.abs(b.z - f.z) < f.d / 2 + 1) {
          b.life = 0
          this.mission.damageTerminal(f.id, E.strayDamage)
          this.puff(b.x, b.z, 0xff9b3d, 1.5)
          this.restoreFacilityLooks()
          if (this.warnT <= 0) { this.hud.toast(`${f.name} hit! Hold fire near the terminal.`, 'warn', 2.5); this.warnT = 3 }
        }
      }
    }
  }

  private restoreFacilityLooks(): void {
    for (const fac of this.mission.terminal.facilities) {
      const mesh = this.facilityMeshes.get(fac.id)
      if (mesh) (mesh.material as THREE.MeshLambertMaterial).color.setHex(0x8f948c).lerp(new THREE.Color(0x2c2623), 1 - fac.integrity)
    }
  }

  // ---------- Falcon cover ----------

  private updateFalcon(dt: number): void {
    const f = this.falconPending
    if (!f) return
    f.t -= dt
    if (Math.random() < 0.6) this.puff(f.x + (Math.random() - 0.5) * E.falconRadius, f.z + (Math.random() - 0.5) * E.falconRadius, 0xffd9a0, 2)
    if (f.t > 0) return
    this.falconPending = null
    this.falconCd = E.falconCd
    for (const e of this.alive()) if (Math.hypot(e.x - f.x, e.z - f.z) < E.falconRadius) this.killHostile(e)
    this.hud.toast('Falcon cover on target.', 'good', 2)
  }

  protected override pauseCard() {
    const card = super.pauseCard()
    return { ...card, title: 'Operation Open Strait', text: `${this.runner.objective} · ${this.mission.reinforcements} reinforcements left` }
  }

  protected hudFrame(): HudFrame {
    const st = this.runner.state, m = this.mission
    const items: HudFrame['items'] = []
    let hint: string | null = null
    if (st === 'E04_BEACH_RECON') {
      items.push({ label: 'North beach marked', done: this.scouted.has('north') }, { label: 'West beach marked', done: this.scouted.has('west') })
      hint = this.raven.flying ? 'Fly Raven over both beaches.' : 'Press Q to fly Raven.'
    } else if (/LANDING/.test(st)) items.push({ label: 'Secure the beachhead', done: false, progress: this.siteProgress('beachhead') })
    else if (/TERMINAL/.test(st)) { items.push({ label: 'Secure the oil terminal', done: false, progress: this.siteProgress('terminal') }); hint = 'Do not fire at jetties, tanks or pumps.' }
    else if (/AIRSTRIP/.test(st)) items.push({ label: 'Take the airstrip', done: false, progress: this.siteProgress('airstrip') })
    else if (st === 'E04_LANE_CLEARANCE') {
      for (const l of m.lanes.lanes) items.push({ label: `${l.id} lane: ${l.cleared}/${l.hazards} cleared (${l.revealed} found)`, done: l.cleared === l.hazards })
      hint = this.raven.flying ? 'Fly Raven over the amber hazard positions along the lanes.' : 'Press Q to fly Raven along the lanes.'
    } else if (st === 'E04_TANKER_ESCORT') items.push({ label: `Tankers through ${m.escort.through}/${m.escort.tankers.length} (lost ${m.escort.lost})`, done: false })
    const cd = this.falconPending ? 'INBOUND' : this.falconCd > 0 ? `${Math.ceil(this.falconCd)}s` : 'READY'
    return this.frameFor({
      eyebrow: 'OPERATION MIDNIGHT HAVOC // EPISODE 04 // HYPOTHETICAL', title: 'OPERATION OPEN STRAIT', phase: st.replace(/^E04_/, '').replace(/_/g, ' '),
      items, hint,
      vitals: [
        { label: 'REINFORCEMENTS', value: String(m.reinforcements), low: m.reinforcements <= 5 },
        { label: 'TERMINAL', bar: m.terminal.integrity, low: m.terminal.integrity < RAW.terminalIntactThreshold }
      ],
      abilities: [{ key: 'Q', name: 'RAVEN', status: this.raven.flying ? 'FLYING' : 'READY' }, { key: 'F', name: 'FALCON', status: m.airSupport ? cd : 'LOCKED' }],
      bounds: BOUNDS,
      lines: [
        { pts: [{ x: -HALF, z: SHORE_Z + 60 }, { x: HALF, z: SHORE_Z + 60 }], color: 'rgba(80,140,200,.35)', width: 130 },
        { pts: this.lanePts.inbound, color: 'rgba(120,200,255,.5)', dash: true }, { pts: this.lanePts.outbound, color: 'rgba(120,200,255,.5)', dash: true }
      ],
      markers: [
        { x: TERMINAL.x, z: TERMINAL.z, color: '#7ec8ff', size: 5, shape: 'square' },
        { x: TOWN.x, z: TOWN.z, color: '#e8e0c0', size: 5, shape: 'square' },
        { x: AIRSTRIP.x, z: AIRSTRIP.z, color: '#78ffab', size: 3, shape: 'diamond' },
        ...(st === 'E04_BEACH_RECON' || st === 'E04_CHOOSE_BEACH' ? [BEACHES.north, BEACHES.west].map(b => ({ ...b, color: '#ffd166', size: 4, shape: 'diamond' as const })) : []),
        ...this.hazards.filter(h => h.revealed && !h.cleared).map(h => ({ x: h.x, z: LANE_Z[h.lane], color: '#ff9b3d', size: 3, shape: 'diamond' as const })),
        ...this.tankers.map(t => ({ x: t.mesh.position.x, z: t.mesh.position.z, color: '#cfd3d6', size: 3, shape: 'square' as const }))
      ]
    })
  }
}

function buildMap(scene: THREE.Scene, facilityMeshes: Map<string, THREE.Mesh>): Collision {
  const collision = new Collision(BOUNDS)
  const solids = new Solids(collision)
  const rand = rng(404)
  scene.background = new THREE.Color(0xcfd6d2)
  scene.fog = new THREE.Fog(0xcfd6d2, 130, 440)
  // Sea and the island.
  groundPatch(scene, 0, 0, HALF * 2 + 200, HALF * 2 + 400, 0x2f6f8f, 0)
  groundPatch(scene, 0, (-HALF + SHORE_Z) / 2, HALF * 2, SHORE_Z + HALF, 0xcdb98c, 0.02)
  groundPatch(scene, AIRSTRIP.x, AIRSTRIP.z, 150, 60, 0xb9a97f, 0.04)
  groundPatch(scene, AIRSTRIP.x, AIRSTRIP.z, 130, 12, 0x55524b, 0.06)
  groundPatch(scene, TERMINAL.x, TERMINAL.z, 120, 120, 0x9a9a92, 0.04)
  groundPatch(scene, TOWN.x, TOWN.z, 130, 100, 0xc2ad82, 0.04)
  groundPatch(scene, BEACHES.north.x, BEACHES.north.z, 110, 40, 0xe2d3a6, 0.05)
  groundPatch(scene, BEACHES.west.x, BEACHES.west.z, 40, 110, 0xe2d3a6, 0.05)
  groundPatch(scene, 0, LANE_Z.inbound, HALF * 2, 6, 0x3b84a8, 0.03)
  groundPatch(scene, 0, LANE_Z.outbound, HALF * 2, 6, 0x3b84a8, 0.03)
  // The sea is not walkable; rounds fly over it.
  collision.add({ x0: -HALF, z0: SHORE_Z, x1: HALF, z1: HALF, h: 0, blocksShots: false, tag: 'sea' })
  const s = (x: number, z: number, w: number, d: number, h: number, c: number) => solids.add(x, z, w, d, h, c)
  s(0, -HALF - 3, HALF * 2 + 12, 6, 8, PALETTE.dark); s(-HALF - 3, 0, 6, HALF * 2, 8, PALETTE.dark); s(HALF + 3, 0, 6, HALF * 2, 8, PALETTE.dark)
  // Oil terminal facilities: shots pass through (rounds that land in them are counted as damage).
  for (const f of FACILITIES) {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(f.w, f.id === 'tank_farm' ? 9 : 6, f.d), new THREE.MeshLambertMaterial({ color: 0x8f948c }))
    mesh.position.set(f.x, f.id === 'tank_farm' ? 4.5 : 3, f.z)
    scene.add(mesh); facilityMeshes.set(f.id, mesh)
    collision.add({ x0: f.x - f.w / 2, z0: f.z - f.d / 2, x1: f.x + f.w / 2, z1: f.z + f.d / 2, h: 6, blocksShots: false, tag: f.id })
  }
  // Kharg town: a block of low houses (protected, no hostiles).
  for (let i = 0; i < 16; i++) {
    const x = TOWN.x - 48 + (i % 4) * 32 + (rand() - 0.5) * 6, z = TOWN.z - 36 + Math.floor(i / 4) * 24 + (rand() - 0.5) * 4
    s(x, z, 14 + rand() * 6, 10 + rand() * 6, 5 + rand() * 4, PALETTE.sand)
  }
  // Airstrip hangars, beach defences and scattered cover.
  s(AIRSTRIP.x - 40, AIRSTRIP.z - 38, 30, 16, 9, PALETTE.steel); s(AIRSTRIP.x + 30, AIRSTRIP.z - 38, 24, 14, 8, PALETTE.steel)
  s(BEACHES.north.x - 20, BEACHES.north.z + 26, 16, 4, 3, PALETTE.sand); s(BEACHES.north.x + 22, BEACHES.north.z + 24, 14, 4, 3, PALETTE.sand)
  s(BEACHES.west.x + 26, BEACHES.west.z - 22, 4, 16, 3, PALETTE.sand); s(BEACHES.west.x + 24, BEACHES.west.z + 24, 4, 14, 3, PALETTE.sand)
  const keep = [BEACHES.north, BEACHES.west, AIRSTRIP, TERMINAL, SEA_START]
  for (let i = 0, tries = 0; i < 70 && tries < 3000; tries++) {
    const w = 6 + rand() * 12, d = 6 + rand() * 12, x = -HALF + 14 + rand() * (HALF * 2 - 28), z = -HALF + 14 + rand() * (SHORE_Z + HALF - 40)
    if (keep.some(p => Math.hypot(x - p.x, z - p.z) < 42 + Math.max(w, d)) || Math.hypot(x - TOWN.x, z - TOWN.z) < 85 || solids.overlaps(x, z, Math.max(w, d) / 2, 4)) continue
    if (FACILITIES.some(f => Math.abs(x - f.x) < f.w / 2 + w && Math.abs(z - f.z) < f.d / 2 + d)) continue
    s(x, z, w, d, 3 + rand() * 6, rand() > 0.5 ? PALETTE.stone : PALETTE.clay)
    i++
  }
  solids.build(scene)
  return collision
}
