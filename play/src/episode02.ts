import * as THREE from 'three'
import {
  ContactManager, DroneController, E02_POINTS, SilentCorridorMission, parseMissionDefinition,
  type E02Carryover, type GardunMapData, type MissionOutcome
} from '../../src/index.js'
import episodeJson from '../../data/missions/episode-02-silent-corridor.json'
import mapJson from '../../data/maps/gardun-city.json'
import { Battle, type BattleOptions, type Friendly, type Hostile } from './engine'
import {
  BOUNDS, CORRIDOR_END, DRAIN, ECHO, MAP_LINES, RELAY_KILO, RIVER_WIDTH, ROUTES, S, SOUTH_GATE,
  buildCorridor, toData, toWorld, type BuiltCorridor
} from './corridor'
import { BASE_CONTROLS, controlsHtml, type CardSpec, type HudFrame } from './hud'

const MAP = mapJson as unknown as GardunMapData
const C = { convoySpeed: 8, escortRange: 110, droneSpeed: 34, droneSight: 70, wreckPush: 3, relayRadius: 25, restoreRadius: 7 }
type VehicleId = 'warden-1' | 'nomad-1' | 'nomad-2'
const SPACING: Record<VehicleId, number> = { 'warden-1': 0, 'nomad-1': 18, 'nomad-2': 36 }
// Hostile hits wear vehicles down per point of damage (the Warden is armoured).
const ARMOUR: Record<VehicleId, number> = { 'warden-1': 0.0022, 'nomad-1': 0.0045, 'nomad-2': 0.0045 }

interface Vehicle { id: VehicleId; x: number; z: number; angle: number; mesh: THREE.Group; friendly: Friendly; fireCd: number; wrecked: boolean }
interface GameSnap { route: 'main' | 'riverbed'; s: number; player: { x: number; z: number }; wreckCleared: boolean; rampOpen: boolean; relay: { x: number; z: number } | null }

export interface Episode02Options extends BattleOptions {
  crewRecovered: boolean
  onResult: (outcome: MissionOutcome, carryover: E02Carryover) => void
}

export class SilentCorridorGame extends Battle {
  readonly mission: SilentCorridorMission
  private readonly contacts = new ContactManager()
  private readonly world: BuiltCorridor
  private readonly vehicles: Vehicle[]
  private route: 'main' | 'riverbed' = 'main'
  private s = 0
  private waiting = false
  private drone = { x: ECHO.x, z: ECHO.z + 6, flying: false, mesh: new THREE.Group(), warnT: 0 }
  private relayPos: { x: number; z: number } | null = null
  private wreckCleared = false
  private rampOpen = false
  private wreckT = 0
  private choosing = false
  private snap: GameSnap | null = null
  private lastState = ''

  constructor(root: HTMLElement, private readonly o: Episode02Options) {
    super(root, o)
    const def = parseMissionDefinition(episodeJson, MAP.sectors.map(s => s.id))
    this.mission = new SilentCorridorMission(def, new DroneController(), this.contacts, undefined, o.crewRecovered)
    this.world = buildCorridor(this.scene)
    this.collision = this.world.collision
    this.respawnDelay = 4
    Object.assign(this.player, { x: ECHO.x + 8, z: ECHO.z - 30 })

    this.vehicles = (['warden-1', 'nomad-1', 'nomad-2'] as VehicleId[]).map(id => this.makeVehicle(id))
    this.buildDrone()
    this.hud.controls = controlsHtml([...BASE_CONTROLS, [['Q'], 'Fly Raven / back to the squad'], [['E'], 'Place relay · use tunnel hatch'], [['1', '2'], 'Pick a route at the bridge']])

    this.mission.runner.events.on('mission:checkpoint', () => {
      this.snap = { route: this.route, s: this.s, player: { x: this.player.x, z: this.player.z }, wreckCleared: this.wreckCleared, rampOpen: this.rampOpen, relay: this.relayPos && { ...this.relayPos } }
      if (this.time > 0) this.hud.toast('Checkpoint saved', 'good')
    })
    this.mission.progress()                 // briefing → form up → Relay Kilo
    this.populate()
    this.placeVehicles()
    this.hud.toast('Escort the Falcon-2 crew and supplies from Checkpoint Echo to South Gate.', 'info', 6)
    this.start()
  }

  // ---------- setup ----------

  private makeVehicle(id: VehicleId): Vehicle {
    const g = new THREE.Group()
    const olive = new THREE.MeshLambertMaterial({ color: id === 'warden-1' ? 0x4c5a3e : 0x5d6648 })
    const dark = new THREE.MeshLambertMaterial({ color: 0x2f3528 })
    if (id === 'warden-1') {
      const hull = new THREE.Mesh(new THREE.BoxGeometry(5.4, 2.2, 9), olive); hull.position.y = 1.5
      const turret = new THREE.Mesh(new THREE.BoxGeometry(3.4, 1.4, 3.8), dark); turret.position.y = 3.2; turret.name = 'turret'
      const barrel = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.5, 6), dark); barrel.position.set(0, 0, 3.8); turret.add(barrel)
      const tracks = new THREE.Mesh(new THREE.BoxGeometry(6, 1, 9.4), dark); tracks.position.y = 0.5
      g.add(tracks, hull, turret)
    } else {
      const bed = new THREE.Mesh(new THREE.BoxGeometry(4.2, 2.6, 6), olive); bed.position.set(0, 2, -1.4)
      const cab = new THREE.Mesh(new THREE.BoxGeometry(4, 2.4, 3), olive); cab.position.set(0, 1.9, 3.4)
      const glass = new THREE.Mesh(new THREE.BoxGeometry(3.6, 0.9, 0.2), new THREE.MeshLambertMaterial({ color: 0x2b3a40 })); glass.position.set(0, 2.5, 4.95)
      const wheels = new THREE.Mesh(new THREE.BoxGeometry(4.6, 1.2, 8.6), dark); wheels.position.y = 0.6
      g.add(wheels, bed, cab, glass)
      if (id === 'nomad-1' && this.o.crewRecovered) {
        const mark = new THREE.Mesh(new THREE.BoxGeometry(2, 0.1, 2), new THREE.MeshBasicMaterial({ color: 0x78ffab })); mark.position.set(0, 3.35, -1.4); g.add(mark)
      }
    }
    const ring = new THREE.Mesh(new THREE.RingGeometry(5.4, 6, 32), new THREE.MeshBasicMaterial({ color: 0x78ffab, transparent: true, opacity: 0.6, side: THREE.DoubleSide }))
    ring.rotation.x = -Math.PI / 2; ring.position.y = 0.1; ring.name = 'ring'
    g.add(ring)
    this.scene.add(g)
    const v: Vehicle = { id, x: 0, z: 0, angle: 0, mesh: g, fireCd: 1, wrecked: false, friendly: null as unknown as Friendly }
    const self = this
    v.friendly = {
      id, r: 3.4,
      get x() { return v.x }, get z() { return v.z }, get alive() { return self.integrity(id) > 0 },
      hit: (d: number) => this.hitVehicle(id, d)
    }
    return v
  }

  private buildDrone(): void {
    const d = this.drone.mesh
    const body = new THREE.Mesh(new THREE.BoxGeometry(1.4, 0.4, 1.4), new THREE.MeshLambertMaterial({ color: 0x2f3d3a }))
    for (const [x, z] of [[-1.3, -1.3], [1.3, -1.3], [-1.3, 1.3], [1.3, 1.3]] as const) {
      const rotor = new THREE.Mesh(new THREE.CylinderGeometry(0.8, 0.8, 0.08, 12), new THREE.MeshBasicMaterial({ color: 0x8fd3ff, transparent: true, opacity: 0.55 }))
      rotor.position.set(x, 0.25, z); d.add(rotor)
    }
    const ring = new THREE.Mesh(new THREE.RingGeometry(2.6, 3.1, 24), new THREE.MeshBasicMaterial({ color: 0x8fd3ff, transparent: true, opacity: 0.7, side: THREE.DoubleSide }))
    ring.rotation.x = -Math.PI / 2; ring.position.y = -13.8; ring.name = 'shadow'
    d.add(body, ring)
    d.position.set(this.drone.x, 14, this.drone.z)
    this.scene.add(d)
  }

  /** Hostiles for the current stretch of the mission. */
  private populate(): void {
    const st = this.mission.state
    if (['E02_RELAY_KILO', 'E02_CORRIDOR_SCOUT', 'E02_CORRIDOR_ADVANCE', 'E02_COMMS_OUTAGE', 'E02_BRIDGE_OBSTRUCTED'].includes(st)) {
      // A roadblock beyond the bridge: the far bank.
      const pts = [[0, 0], [10, -6], [-8, 8], [14, 10], [-4, -14], [24, -2]]
      pts.forEach(([dx, dz]) => this.spawnHostile(CORRIDOR_END.x + dx!, CORRIDOR_END.z + dz!, { tag: 'farbank', anchor: CORRIDOR_END, leash: 38, force: true }))
    }
    if (st === 'E02_SOUTH_GATE_APPROACH') this.spawnYard()
  }

  private spawnYard(): void {
    const pts = [[0, -10], [-18, -6], [18, -4], [-10, 10], [12, 12], [0, 18]]
    pts.forEach(([dx, dz]) => this.spawnHostile(SOUTH_GATE.x + dx!, SOUTH_GATE.z + dz!, { tag: 'yard', anchor: SOUTH_GATE, leash: 36, force: true }))
    this.world.yardMarker.visible = true
  }

  // ---------- convoy ----------

  private integrity(id: VehicleId): number { return this.mission.convoy.vehicles.find(v => v.id === id)?.integrity ?? 0 }
  private path() { return ROUTES[this.route] }

  private hitVehicle(id: VehicleId, damage: number): void {
    if (this.integrity(id) <= 0) return
    this.mission.damageVehicle(id, damage * ARMOUR[id])
    if (this.integrity(id) <= 0) {
      const v = this.vehicles.find(x => x.id === id)!
      this.puff(v.x, v.z, 0x2a2622, 7)
      this.sound.boom()
      const cargo = this.mission.convoy.vehicles.find(x => x.id === id)?.cargo
      this.hud.toast(`${id.toUpperCase()} destroyed${cargo === 'FALCON_CREW' ? ': the Falcon-2 crew is lost' : cargo === 'SUPPLIES' ? ': supplies lost' : ''}`, 'warn', 4)
    }
    this.checkEnd()
  }

  /** How far along the route the lead vehicle may go in the current state. */
  private moveTarget(): number {
    const m = this.mission, st = m.state
    if (st === 'E02_CORRIDOR_ADVANCE') return S.outage
    if (st === 'E02_COMMS_OUTAGE') return m.runner.hasFlag('relay_kilo_restored') ? S.approach : this.s
    if (st === 'E02_RIVERBED_DETOUR') return S.narrowsHold
    if (st === 'E02_BRIDGE_CLEARED') return S.bridgeWreck
    if (st === 'E02_SOUTH_GATE_APPROACH') return S.end[this.route]
    return this.s
  }

  private updateConvoy(dt: number): void {
    const lead = this.path().at(this.s)
    const target = this.moveTarget()
    this.waiting = false
    if (this.s < target - 0.01) {
      if (this.player.alive && Math.hypot(this.player.x - lead.x, this.player.z - lead.z) > C.escortRange) this.waiting = true
      else this.s = Math.min(target, this.s + C.convoySpeed * dt)
    }
    this.placeVehicles()
    // Warden-1 engages hostiles it can see.
    const w = this.vehicles[0]!
    if (this.integrity('warden-1') > 0 && (w.fireCd -= dt) <= 0) {
      let best: Hostile | undefined, bd = 58
      for (const e of this.enemies) {
        if (!e.alive) continue
        const d = Math.hypot(e.x - w.x, e.z - w.z)
        if (d < bd && this.collision.clear(w.x, w.z, e.x, e.z)) { bd = d; best = e }
      }
      w.fireCd = 1.4
      if (best) {
        const a = Math.atan2(best.x - w.x, best.z - w.z)
        const turret = w.mesh.getObjectByName('turret')!
        turret.rotation.y = a - w.angle
        this.fire(w.x, w.z, a + (Math.random() - 0.5) * 0.08, true, 45)
        this.sound.shot(0.5)
      }
    }
  }

  private placeVehicles(): void {
    const path = this.path()
    for (const v of this.vehicles) {
      const alive = this.integrity(v.id) > 0
      if (!alive) {
        if (!v.wrecked) { v.wrecked = true; v.mesh.traverse(o => { if (o instanceof THREE.Mesh && o.name !== 'ring') (o.material as THREE.MeshLambertMaterial).color?.setHex(0x2a2622) }); v.mesh.getObjectByName('ring')!.visible = false }
        continue
      }
      const p = path.at(Math.max(0, this.s - SPACING[v.id]))
      v.x = p.x; v.z = p.z; v.angle = p.angle
      v.mesh.position.set(p.x, 0, p.z)
      v.mesh.rotation.y = p.angle
    }
  }

  protected override pushOut(p: { x: number; z: number }, r: number): void {
    for (const v of this.vehicles) {
      const dx = p.x - v.x, dz = p.z - v.z, d = Math.hypot(dx, dz), min = r + 3.2
      if (d < min && d > 1e-3) { p.x = v.x + (dx / d) * min; p.z = v.z + (dz / d) * min }
    }
  }

  protected override friendlies(): Friendly[] { return [this.playerFriendly, ...this.vehicles.map(v => v.friendly)] }

  // ---------- Raven ----------

  protected override steeringOther(): boolean { return this.drone.flying }
  protected override focus(): { x: number; z: number } { return this.drone.flying ? this.drone : this.player }
  protected override cameraHeight(): number { return this.drone.flying ? 115 : 82 }

  private linkAt(x: number, z: number) { return this.mission.relays.classify(toData(x, z)) }

  private updateDrone(dt: number): void {
    const d = this.drone
    d.warnT -= dt
    if (d.flying) {
      const [mx, mz] = this.moveInput()
      const ml = Math.hypot(mx, mz)
      if (ml > 0) {
        const nx = d.x + (mx / ml) * C.droneSpeed * dt, nz = d.z + (mz / ml) * C.droneSpeed * dt
        if (this.linkAt(nx, nz) === 'LOST') {
          if (d.warnT <= 0) { this.hud.toast('Raven link lost beyond this point. Place a relay to reach further.', 'warn', 2.5); d.warnT = 3 }
        } else { d.x = Math.max(BOUNDS.x0, Math.min(BOUNDS.x1, nx)); d.z = Math.max(BOUNDS.z0, Math.min(BOUNDS.z1, nz)) }
      }
      // Raven marks hostiles it can see.
      for (const e of this.enemies) if (e.alive && Math.hypot(e.x - d.x, e.z - d.z) < C.droneSight) {
        this.contacts.upsert({ id: e.id, classification: 'GAME_UNIT', position: toData(e.x, e.z), source: 'RAVEN', time: this.time * 1000, quality: this.mission.relays.signalAt(toData(d.x, d.z)) })
        this.contacts.confirm(e.id)
      }
      if (this.mission.state === 'E02_CORRIDOR_SCOUT' && Math.hypot(d.x - CORRIDOR_END.x, d.z - CORRIDOR_END.z) < 16) this.scout()
    } else {
      // Hovers over the squad.
      d.x += (this.player.x + 4 - d.x) * Math.min(1, dt * 3); d.z += (this.player.z + 4 - d.z) * Math.min(1, dt * 3)
    }
    d.mesh.position.set(d.x, 14, d.z)
    d.mesh.rotation.y += dt * 0.6
  }

  private toggleDrone(): void {
    const d = this.drone
    if (!d.flying && !this.player.alive) return
    d.flying = !d.flying
    this.hud.toast(d.flying ? 'Flying Raven. The squad holds position. Press Q to return.' : 'Back with the squad.', 'info', 2.5)
  }

  private placeRelay(): void {
    const d = this.drone, m = this.mission
    if (m.state !== 'E02_RELAY_KILO') return
    if (!d.flying) return this.hud.toast('Fly Raven (Q) to the blue ring to drop Relay Kilo.', 'info', 3)
    if (Math.hypot(d.x - RELAY_KILO.x, d.z - RELAY_KILO.z) > C.relayRadius) return this.hud.toast('Drop Relay Kilo inside the blue ring on the valley shoulder.', 'warn', 3)
    if (!m.placeRelayKilo(toData(d.x, d.z))) return this.hud.toast('No link here: Relay Kilo needs the drone link to reach it.', 'warn', 3)
    this.relayPos = { x: d.x, z: d.z }
    this.world.relayMesh.position.set(d.x, 0, d.z); this.world.relayMesh.visible = true
    this.world.relayZone.visible = false
    this.sound.chime(true)
    this.hud.toast('Relay Kilo online. Now scout the corridor end (amber beacon).', 'good', 4)
    m.progress()
  }

  private scout(): void {
    const d = this.drone, m = this.mission
    const seen = this.enemies.filter(e => e.alive && Math.hypot(e.x - d.x, e.z - d.z) < C.droneSight).map(e => ({ id: e.id, position: toData(e.x, e.z) }))
    if (!m.scoutCorridor(seen, toData(d.x, d.z), this.time * 1000)) {
      if (d.warnT <= 0) { this.hud.toast('Link too weak to send back what Raven sees.', 'warn', 2.5); d.warnT = 3 }
      return
    }
    m.runner.advance()                       // scout → advance (convoy departs)
    this.world.endMarker.visible = false
    d.flying = false
    this.sound.chime(true)
    this.hud.toast(`Corridor scouted: ${seen.length} hostiles marked beyond the bridge. Convoy moving. Stay close.`, 'good', 5)
  }

  // ---------- mission steps ----------

  protected override onKey(k: string): void {
    if (k === 'q') this.toggleDrone()
    if (k === 'e') { if (this.mission.state === 'E02_RELAY_KILO') this.placeRelay(); else this.useHatch() }
  }

  private useHatch(): void {
    if (this.mission.state !== 'E02_DRAINAGE_INFILTRATION' || this.drone.flying) return
    const p = this.player
    for (const [a, b, off] of [[DRAIN.a, DRAIN.b, -5], [DRAIN.b, DRAIN.a, 5]] as const) {
      if (Math.hypot(p.x - a.x, p.z - a.z) < 6) { p.x = b.x + off; p.z = b.z + off; this.hud.toast('Through the drainage tunnel', 'info', 1.5); return }
    }
    this.hud.toast('Stand on a hatch (amber ring) to use the drainage tunnel.', 'info', 2.5)
  }

  private showChoice(): void {
    if (this.choosing || this.over) return
    this.choosing = true
    this.paused = true
    this.keys.clear(); this.mouse.down = false
    const pick = (route: 'riverbed_detour' | 'drainage_infiltration') => { this.choosing = false; this.paused = false; this.hud.card(null); this.clock.getDelta(); this.choose(route) }
    this.hud.card({
      eyebrow: 'DECISION · BRIDGE OBSTRUCTED', title: 'The bridge is blocked', text: this.mission.runner.objective,
      options: [
        { label: 'Riverbed detour', detail: 'The whole convoy drives the Dry Riverbed. Expect an ambush in the narrows. Keep Nomad-1 alive.', action: () => pick('riverbed_detour') },
        { label: 'Drainage infiltration', detail: 'You go alone through the drainage tunnel and clear the far bank. Then Warden-1 pushes the wreck off the bridge.', action: () => pick('drainage_infiltration') }
      ]
    })
    const keyPick = (e: KeyboardEvent) => {
      if (!this.choosing) { window.removeEventListener('keydown', keyPick); return }
      if (e.key === '1') { window.removeEventListener('keydown', keyPick); pick('riverbed_detour') }
      if (e.key === '2') { window.removeEventListener('keydown', keyPick); pick('drainage_infiltration') }
    }
    window.addEventListener('keydown', keyPick)
  }

  protected override setPaused(p: boolean): void {
    if (this.choosing) return
    super.setPaused(p)
  }

  private choose(route: 'riverbed_detour' | 'drainage_infiltration'): void {
    const m = this.mission
    m.chooseRoute(route)
    if (route === 'riverbed_detour') {
      this.route = 'riverbed'
      this.rampOpen = true
      this.collision.remove(this.world.rampGate.box); this.world.rampGate.mesh.visible = false
      // The ambush the mission placed in the narrows, plus riflemen on the banks.
      for (const c of this.contacts.all()) if (c.id.startsWith('e02-ambush') && m.remainingAmbush.includes(c.id)) {
        const w = toWorld(c.worldPosition)
        this.spawnHostile(w.x, w.z, { id: c.id, tag: 'ambush', anchor: w, leash: 26, force: true })
      }
      const n = toWorld(E02_POINTS.ambushNarrows)
      this.spawnHostile(n.x - 20, n.z - 10, { tag: 'ambush-support', anchor: n, leash: 26, force: true })
      this.spawnHostile(n.x + 24, n.z + 18, { tag: 'ambush-support', anchor: n, leash: 26, force: true })
      this.hud.toast('Riverbed detour. Ambush expected in the narrows.', 'warn', 4)
    } else {
      this.world.drainHatches.forEach(h => { h.visible = true })
      this.hud.toast('Drainage infiltration. Get to the hatch by the bridge (E) and clear the far bank.', 'warn', 5)
    }
  }

  protected override onHostileDown(e: Hostile): void {
    const m = this.mission
    this.contacts.loseTracking(e.id, this.time * 1000)
    if (e.id.startsWith('e02-ambush') && m.clearAmbushContact(e.id, this.time * 1000)) {
      if (!m.remainingAmbush.length) { m.progress(); this.hud.toast('Ambush repelled. Convoy moving to South Gate.', 'good', 4) }
    }
    if (e.tag === 'farbank' && m.state === 'E02_DRAINAGE_INFILTRATION' && !this.alive('farbank').length) {
      m.clearFarBank(); m.progress()
      this.hud.toast('Far bank clear. Warden-1 is moving up to push the wreck off the bridge.', 'good', 4)
    }
  }

  /** Steps the engine reports into the mission each frame. */
  private updateMission(dt: number): void {
    const m = this.mission, st = m.state, p = this.player
    if (st === 'E02_CORRIDOR_ADVANCE' && this.s >= S.outage - 0.5) {
      m.runner.advance()                     // → communications outage
      this.sound.chime(false)
      this.hud.toast('Communications outage. Every relay is jammed. Get to Relay Kilo and restore it.', 'warn', 5)
      this.drone.flying = false
      const lead = this.path().at(this.s)
      for (const [dx, dz] of [[-40, -20], [-44, 8], [40, -28], [46, 4]] as const) this.spawnHostile(lead.x + dx, lead.z + dz, { tag: 'outage', anchor: lead, leash: 45 })
    }
    if (st === 'E02_COMMS_OUTAGE') {
      if (!m.runner.hasFlag('relay_kilo_restored') && this.relayPos && p.alive && Math.hypot(p.x - this.relayPos.x, p.z - this.relayPos.z) < C.restoreRadius) {
        if (m.restoreRelayKilo(toData(p.x, p.z))) {
          m.convoy.resume()
          this.sound.chime(true)
          this.hud.toast('Relay Kilo restored. Convoy moving to the bridge.', 'good', 4)
        }
      }
      if (m.runner.hasFlag('relay_kilo_restored') && this.s >= S.approach - 0.5) {
        m.runner.advance()                   // → bridge obstructed (checkpoint)
        this.sound.chime(false)
      }
    }
    if (m.state === 'E02_BRIDGE_OBSTRUCTED') this.showChoice()
    if (st === 'E02_BRIDGE_CLEARED' && !this.wreckCleared && this.s >= S.bridgeWreck - 0.5) {
      if (this.integrity('warden-1') <= 0) return
      this.wreckT += dt
      if (this.wreckT >= C.wreckPush) {
        m.clearBridge()
        if (m.runner.hasFlag('bridge_cleared')) {
          this.wreckCleared = true
          this.collision.remove(this.world.wreck.box)
          this.world.wreck.mesh.position.x += 18; this.world.wreck.mesh.rotation.z = 1.2
          m.progress()
          this.hud.toast('Bridge clear. Convoy crossing to South Gate.', 'good', 4)
        }
      }
    }
    if (m.state === 'E02_SOUTH_GATE_APPROACH') {
      if (this.lastState !== 'E02_SOUTH_GATE_APPROACH' && !this.alive('yard').length) this.spawnYard()
      const lead = this.path().at(this.s)
      if (!this.alive('yard').length && Math.hypot(lead.x - SOUTH_GATE.x, lead.z - SOUTH_GATE.z) < 40) {
        m.clearStaging(); m.progress()
        m.finish()
        this.checkEnd()
      }
    }
    this.lastState = m.state
  }

  private checkEnd(): void {
    const m = this.mission
    if (m.state === 'E02_CONVOY_LOST') {
      this.endScreen({
        eyebrow: 'CONVOY LOST', eyebrowKind: 'warn', title: m.convoy.carriesCrew ? 'The Falcon-2 crew is lost' : 'Every Nomad is lost',
        text: m.runner.objective, stats: this.statsBlock(),
        buttons: [{ label: 'Restore checkpoint', primary: true, action: () => this.restoreCheckpoint() }, { label: 'Quit to menu', action: () => this.exit() }]
      })
      return
    }
    if (m.runner.outcome && m.runner.outcome !== 'FAILED') {
      const carry = m.carryover()
      this.o.onResult(m.runner.outcome, carry)
      const full = m.runner.outcome === 'SUCCESS'
      this.endScreen({
        eyebrow: full ? 'OPERATION COMPLETE' : 'OPERATION COMPLETE · WITH LOSSES', eyebrowKind: full ? 'good' : 'warn',
        title: 'South Gate staging established',
        text: `${carry.crewDelivered ? 'Falcon-2 crew delivered' : 'No Falcon-2 crew delivered'}, ${carry.nomadsRemaining} Nomad${carry.nomadsRemaining === 1 ? '' : 's'} made it, Warden-1 ${carry.wardenOperational ? 'operational' : 'lost'}. Episode 03 starts with ${20 + 5 * carry.nomadsRemaining} reinforcements${carry.crewDelivered ? ' and Falcon air support once the Airfield falls' : ''}.`,
        stats: this.statsBlock(),
        buttons: [{ label: 'Continue to Episode 03', primary: true, action: () => this.exit() }]
      })
    }
  }

  protected override respawnPoint(): { x: number; z: number } {
    const lead = this.path().at(Math.max(0, this.s - 50))
    return { x: lead.x + 6, z: lead.z + 6 }
  }

  protected override onPlayerDown(): void { this.drone.flying = false }

  protected restoreCheckpoint(): void {
    const m = this.mission
    if (!this.snap || !m.runner.restoreCheckpoint()) return
    const snap = this.snap
    this.clearHostiles(); this.clearBullets()
    this.route = snap.route; this.s = snap.s
    this.choosing = false
    this.wreckT = 0
    if (snap.rampOpen !== this.rampOpen) {
      this.rampOpen = snap.rampOpen
      if (this.rampOpen) this.collision.remove(this.world.rampGate.box); else this.collision.add(this.world.rampGate.box)
      this.world.rampGate.mesh.visible = !this.rampOpen
    }
    this.world.drainHatches.forEach(h => { h.visible = false })
    this.relayPos = snap.relay && { ...snap.relay }
    this.world.relayMesh.visible = Boolean(this.relayPos)
    if (this.relayPos) this.world.relayMesh.position.set(this.relayPos.x, 0, this.relayPos.z)
    this.world.relayZone.visible = !this.relayPos
    this.world.endMarker.visible = ['E02_RELAY_KILO', 'E02_CORRIDOR_SCOUT'].includes(m.state)
    this.world.yardMarker.visible = m.state === 'E02_SOUTH_GATE_APPROACH'
    for (const v of this.vehicles) if (v.wrecked && this.integrity(v.id) > 0) {
      v.wrecked = false
      this.scene.remove(v.mesh)
      const fresh = this.makeVehicle(v.id)
      Object.assign(v, { mesh: fresh.mesh, friendly: fresh.friendly })
    }
    Object.assign(this.player, { x: snap.player.x, z: snap.player.z })
    this.drone.flying = false
    this.lastState = m.state
    this.resume()
    this.populate()
    this.placeVehicles()
    this.respawn()
    Object.assign(this.player, { x: snap.player.x, z: snap.player.z })
    this.hud.toast('Restored from checkpoint', 'good', 2.5)
  }

  // ---------- frame ----------

  protected update(dt: number): void {
    this.updateDrone(dt)
    this.updateConvoy(dt)
    this.updateMission(dt)
  }

  protected override pauseCard(): CardSpec {
    return { ...super.pauseCard(), title: 'Operation Silent Corridor', text: this.mission.runner.objective }
  }

  private stepLabel(): string {
    const labels: Record<string, string> = {
      E02_RELAY_KILO: 'RELAY KILO', E02_CORRIDOR_SCOUT: 'SCOUT THE CORRIDOR', E02_CORRIDOR_ADVANCE: 'CONVOY ADVANCING',
      E02_COMMS_OUTAGE: 'COMMUNICATIONS OUTAGE', E02_BRIDGE_OBSTRUCTED: 'BRIDGE OBSTRUCTED', E02_RIVERBED_DETOUR: 'RIVERBED DETOUR',
      E02_DRAINAGE_INFILTRATION: 'DRAINAGE INFILTRATION', E02_BRIDGE_CLEARED: 'CLEARING THE BRIDGE', E02_SOUTH_GATE_APPROACH: 'SOUTH GATE',
      E02_STAGING_ESTABLISHED: 'STAGING', E02_COMPLETE: 'COMPLETE', E02_COMPLETE_WITH_LOSSES: 'COMPLETE', E02_CONVOY_LOST: 'CONVOY LOST'
    }
    return labels[this.mission.state] ?? ''
  }

  private hint(): string | null {
    const st = this.mission.state, d = this.drone
    if (this.waiting) return 'The convoy is waiting for you. Stay within escort range.'
    if (st === 'E02_RELAY_KILO') return d.flying ? 'Fly to the blue ring and press E to drop Relay Kilo.' : 'Press Q to fly Raven.'
    if (st === 'E02_CORRIDOR_SCOUT') return d.flying ? 'Fly Raven to the amber beacon beyond the bridge.' : 'Press Q to fly Raven to the corridor end.'
    if (st === 'E02_COMMS_OUTAGE' && !this.mission.runner.hasFlag('relay_kilo_restored')) return 'Run to Relay Kilo (blue mast) to restore it.'
    if (st === 'E02_DRAINAGE_INFILTRATION') return 'Use the hatch by the bridge (E), then clear the far bank.'
    if (st === 'E02_BRIDGE_CLEARED' && this.integrity('warden-1') <= 0) return 'Warden-1 is lost, so the bridge cannot be cleared. Pause and quit, or restore from the failure screen.'
    return null
  }

  protected hudFrame(): HudFrame {
    const m = this.mission, p = this.player, d = this.drone, flags = m.runner.flags, hist = m.runner.history
    const crew = m.convoy.carriesCrew
    const veh = (id: VehicleId, label: string) => ({ label, bar: this.integrity(id), low: this.integrity(id) < 0.35 })
    const link = this.linkAt(d.flying ? d.x : p.x, d.flying ? d.z : p.z)
    const routeChosen = hist.includes('E02_RIVERBED_DETOUR') ? 'riverbed' : hist.includes('E02_DRAINAGE_INFILTRATION') ? 'drainage' : null
    const items = [
      { label: 'Place Relay Kilo', done: flags.includes('relay_kilo_online') },
      { label: 'Scout the corridor', done: flags.includes('corridor_scouted') },
      { label: 'Restore Relay Kilo', done: flags.includes('relay_kilo_restored') },
      routeChosen === 'riverbed' ? { label: `Repel the ambush (${3 - m.remainingAmbush.length}/3)`, done: flags.includes('ambush_repelled') }
        : routeChosen === 'drainage' ? { label: `Clear the far bank (${this.alive('farbank').length} left)`, done: flags.includes('far_bank_clear') }
        : { label: 'Get past the bridge', done: false },
      { label: 'Clear the South Gate yard', done: flags.includes('staging_clear') }
    ]
    const markers = [
      ...(this.relayPos ? [{ ...this.relayPos, color: m.relays.relays[0]?.state === 'JAMMED' ? '#ff5e63' : '#8fd3ff', shape: 'diamond' as const, size: 4 }] : [{ ...RELAY_KILO, color: '#8fd3ff', shape: 'ring' as const, size: 6 }]),
      ...(m.state === 'E02_CORRIDOR_SCOUT' || m.state === 'E02_RELAY_KILO' ? [{ ...CORRIDOR_END, color: '#ffd166', size: 4 }] : []),
      { ...SOUTH_GATE, color: '#ffd166', shape: 'ring' as const, size: 5 },
      ...(m.state === 'E02_DRAINAGE_INFILTRATION' ? [{ ...DRAIN.a, color: '#ffd166', size: 3 }, { ...DRAIN.b, color: '#ffd166', size: 3 }] : []),
      ...(!this.wreckCleared ? [{ x: 380, z: -350, color: '#ff5e63', shape: 'square' as const, size: 2 }] : []),
      ...this.vehicles.map(v => ({ x: v.x, z: v.z, color: this.integrity(v.id) > 0 ? '#78ffab' : '#555', shape: 'square' as const, size: 2.5 })),
      ...(d.flying ? [{ x: d.x, z: d.z, color: '#8fd3ff', size: 3 }] : [])
    ]
    return {
      eyebrow: 'PROJECT PAKTIA // EPISODE 02', title: 'OPERATION SILENT CORRIDOR', phase: this.stepLabel(), objective: m.runner.objective, items,
      vitals: [
        veh('warden-1', 'WARDEN-1'),
        veh('nomad-1', crew ? 'NOMAD-1 · CREW' : 'NOMAD-1 · SUPPLIES'),
        veh('nomad-2', 'NOMAD-2 · SUPPLIES'),
        { label: d.flying ? 'RAVEN LINK' : 'LINK HERE', value: link, low: link === 'LOST' || link === 'CRITICAL' }
      ],
      hp: Math.max(0, p.hp) / 100, mag: p.mag, magSize: this.mag, reloading: p.reload > 0, auto: p.auto,
      abilities: [
        { key: 'Q', name: 'RAVEN', status: d.flying ? 'FLYING' : 'READY' },
        { key: 'E', name: m.state === 'E02_RELAY_KILO' ? 'DROP RELAY' : 'HATCH', status: m.state === 'E02_RELAY_KILO' ? (d.flying ? 'READY' : '—') : m.state === 'E02_DRAINAGE_INFILTRATION' ? 'READY' : '—' }
      ],
      status: m.state === 'E02_COMMS_OUTAGE' ? 'COMMS OUTAGE' : null,
      hint: this.hint(),
      map: {
        bounds: BOUNDS,
        lines: [
          { pts: MAP_LINES.river, color: 'rgba(160,130,80,.45)', width: RIVER_WIDTH },
          { pts: MAP_LINES.road, color: 'rgba(220,233,227,.35)', width: 2 },
          ...(m.state === 'E02_DRAINAGE_INFILTRATION' ? [{ pts: [DRAIN.a, DRAIN.b], color: '#ffd166', dash: true }] : [])
        ],
        markers,
        player: { x: p.x, z: p.z, angle: p.angle },
        enemies: this.enemies.filter(e => e.alive).map(e => ({ x: e.x, z: e.z, confirmed: this.contacts.get(e.id)?.state === 'CONFIRMED' })),
        reveal: d.flying ? C.droneSight : 90,
        revealFrom: d.flying ? { x: d.x, z: d.z } : { x: p.x, z: p.z },
        interference: m.relays.interference
      }
    }
  }
}
