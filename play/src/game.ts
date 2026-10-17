import * as THREE from 'three'
import {
  AegisSupportController, BrokenCitadelMission, ContactManager, ProtectedZone,
  type E02Carryover, type Episode03Data, type GardunMapData, type StrongholdData, type SurpriseModifiers
} from '../../src/index.js'
import episodeJson from '../../data/missions/episode-03-broken-citadel.json'
import strongholdJson from '../../data/missions/operation-broken-citadel.json'
import mapJson from '../../data/maps/gardun-city.json'
import kr47Json from '../../data/weapons/kr47.json'
import { buildWorld, CAPTURE_RADIUS, CELLARS, OBJECTIVES, SOUTH_GATE, toData, type BuiltWorld } from './world'
import { Hud, type HudFrame } from './hud'
import { Sound } from './sound'

export const EPISODE = episodeJson as unknown as Episode03Data
export const STRONGHOLD = strongholdJson as unknown as StrongholdData
const MAP = mapJson as unknown as GardunMapData

// Gameplay tuning for the browser prototype (not part of the shared game data).
const T = {
  playerSpeed: 15, sprintSpeed: 23, playerHp: 100, regenDelay: 4, regenRate: 14,
  fireInterval: 0.095, reloadTime: 1.8, playerDamage: 22, playerSpread: 0.025, bulletSpeed: 190,
  enemyHp: 60, enemySpeed: 5.5, enemyDamage: 7, enemyBulletSpeed: 95, detect: 54,
  captureTime: 6, decayTime: 10, respawnDelay: 3,
  waveEvery: 34, enemyCap: 30, falconCooldown: 40, falconRadius: 16, aegisCooldown: 25, aegisDelay: 2.5
}
const GARRISON: Record<number, number> = { 1: 4, 2: 4, 3: 4, 4: 5, 5: 3 }
const FORCE = '__restore__'

interface Enemy {
  id: string; x: number; z: number; hp: number; angle: number; anchor: { x: number; z: number }
  cooldown: number; sees: boolean; lastSeen: number; lastKnown: { x: number; z: number } | null
  wander: { x: number; z: number }; wanderT: number; seenFor: number; mesh: THREE.Group; alive: boolean
}
interface Bullet { x: number; z: number; vx: number; vz: number; life: number; damage: number; player: boolean; mesh: THREE.Mesh }
interface Fx { mesh: THREE.Object3D; life: number; max: number; grow: number }

export interface GameOptions { carryover: E02Carryover; seed: string; onExit: () => void }

export class Game {
  readonly mission: BrokenCitadelMission
  private readonly contacts = new ContactManager()
  private readonly zones: ProtectedZone[]
  private readonly scene = new THREE.Scene()
  private readonly camera = new THREE.PerspectiveCamera(50, 1, 0.5, 900)
  private readonly renderer: THREE.WebGLRenderer
  private readonly world: BuiltWorld
  private readonly hud: Hud
  private readonly sound = new Sound()
  private readonly clock = new THREE.Clock()
  private readonly sun: THREE.DirectionalLight
  private readonly hemi: THREE.HemisphereLight
  private readonly torch: THREE.SpotLight
  private readonly sand: THREE.Points

  private player = { x: SOUTH_GATE.x, z: SOUTH_GATE.z, hp: T.playerHp, angle: Math.PI, mag: 30, reload: 0, fireCd: 0, lastHit: -99, alive: true, respawn: 0, auto: true }
  private readonly playerMesh: THREE.Group
  private enemies: Enemy[] = []
  private bullets: Bullet[] = []
  private fx: Fx[] = []
  private progress = new Map<string, number>()
  private keys = new Set<string>()
  private mouse = { x: 0, y: 0, down: false, ground: new THREE.Vector3() }
  private time = 0
  private lastPhase: number | undefined
  private lastUnlocks = new Set<string>()
  private lastSurprise: string | undefined
  private mods: SurpriseModifiers = { droneSensorRangeScale: 1, relayInterference: 0, openedRoutes: [], blockedRoutes: [], visibilityScale: 1 }
  private waveT = T.waveEvery
  private falcon = { cd: 0, heli: null as null | { mesh: THREE.Group; from: THREE.Vector3; to: THREE.Vector3; t: number; dur: number; target: { x: number; z: number }; struck: boolean } }
  private aegisReq = { cd: 0, pending: null as null | { id: string; t: number; marker: THREE.Mesh } }
  private stats = { kills: 0, deaths: 0, start: 0 }
  private enemySeq = 0
  private sensorT = 0
  private paused = false
  private over = false
  private raf = 0
  private readonly mag: number
  private readonly disposers: (() => void)[] = []

  constructor(private readonly root: HTMLElement, private readonly opts: GameOptions) {
    this.mag = kr47Json.magazineCapacity
    this.player.mag = this.mag
    this.player.auto = kr47Json.fireModes.includes('automatic')
    const aegis = new AegisSupportController(this.contacts)
    this.mission = new BrokenCitadelMission(EPISODE, STRONGHOLD, MAP, aegis, opts.carryover, opts.seed)
    this.zones = EPISODE.protectedZones.map(z => new ProtectedZone(z.id, z.type, z.position, z.radius))

    this.renderer = new THREE.WebGLRenderer({ antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
    root.appendChild(this.renderer.domElement)
    this.renderer.domElement.className = 'stage'

    this.scene.background = new THREE.Color(0xd8c9a6)
    this.scene.fog = new THREE.Fog(0xd8c9a6, 120, 420)
    this.hemi = new THREE.HemisphereLight(0xfff4dc, 0x6b5a40, 1.15)
    this.sun = new THREE.DirectionalLight(0xffe2b0, 1.6)
    this.sun.position.set(-120, 200, 80)
    this.torch = new THREE.SpotLight(0xfff1c4, 0, 70, 0.55, 0.5, 1)
    this.scene.add(this.hemi, this.sun, this.torch, this.torch.target)

    const sandGeo = new THREE.BufferGeometry()
    const pts = new Float32Array(1500 * 3)
    for (let i = 0; i < pts.length; i += 3) { pts[i] = (Math.random() - 0.5) * 160; pts[i + 1] = Math.random() * 30; pts[i + 2] = (Math.random() - 0.5) * 160 }
    sandGeo.setAttribute('position', new THREE.BufferAttribute(pts, 3))
    this.sand = new THREE.Points(sandGeo, new THREE.PointsMaterial({ color: 0xd9b27a, size: 0.6, transparent: true, opacity: 0.7 }))
    this.sand.visible = false
    this.scene.add(this.sand)

    this.world = buildWorld(this.scene)
    this.playerMesh = this.soldier(0x4f6b45, 0x2f3d2a, 0x78ffab)
    this.scene.add(this.playerMesh)

    this.hud = new Hud(root, {
      onResume: () => this.setPaused(false),
      onRestore: () => this.restoreCheckpoint(),
      onExit: () => { this.dispose(); opts.onExit() }
    })
    this.mission.runner.events.on('mission:checkpoint', ({ checkpoint }) => {
      if (checkpoint.state.startsWith('E03_PHASE')) this.hud.toast('Checkpoint saved', 'good')
    })

    this.bindInput()
    this.resize()
    this.stats.start = performance.now()
    this.mission.beginAssault()
    this.sync()
    this.hud.toast(EPISODE.briefing, 'info', 7)
    this.loop()
    // Dev-server only (removed from production builds): lets automated playtests drive the game.
    if (import.meta.env.DEV) (window as unknown as Record<string, unknown>).__paktia = this
  }

  // ---------- setup helpers ----------

  private soldier(body: number, dark: number, ring?: number): THREE.Group {
    const g = new THREE.Group()
    g.scale.setScalar(1.45)
    if (ring !== undefined) {
      const r = new THREE.Mesh(new THREE.RingGeometry(1.1, 1.45, 24), new THREE.MeshBasicMaterial({ color: ring, transparent: true, opacity: 0.85, side: THREE.DoubleSide }))
      r.rotation.x = -Math.PI / 2; r.position.y = 0.08
      g.add(r)
    }
    const torso = new THREE.Mesh(new THREE.CylinderGeometry(0.75, 0.85, 2.2, 10), new THREE.MeshLambertMaterial({ color: body }))
    torso.position.y = 1.3
    const head = new THREE.Mesh(new THREE.SphereGeometry(0.55, 12, 10), new THREE.MeshLambertMaterial({ color: dark }))
    head.position.y = 2.75
    const rifle = new THREE.Mesh(new THREE.BoxGeometry(0.22, 0.22, 2), new THREE.MeshLambertMaterial({ color: 0x1f1f1f }))
    rifle.position.set(0.45, 1.6, -1.0)
    const pack = new THREE.Mesh(new THREE.BoxGeometry(1.1, 1.1, 0.5), new THREE.MeshLambertMaterial({ color: dark }))
    pack.position.set(0, 1.5, 0.75)
    g.add(torso, head, rifle, pack)
    return g
  }

  private bindInput(): void {
    const on = <K extends keyof WindowEventMap>(t: K, fn: (e: WindowEventMap[K]) => void) => { window.addEventListener(t, fn); this.disposers.push(() => window.removeEventListener(t, fn)) }
    on('resize', () => this.resize())
    on('keydown', e => {
      const k = e.key.toLowerCase()
      if (k === 'escape') { if (!this.over) this.setPaused(!this.paused); return }
      if (this.paused || this.over) return
      this.keys.add(k)
      if (k === 'r') this.startReload()
      if (k === 'b') { this.player.auto = !this.player.auto; this.hud.toast(`KR-47: ${this.player.auto ? 'automatic' : 'single'}`, 'info', 1.5) }
      if (k === 'f') this.callFalcon()
      if (k === 'g') this.callAegis()
      if (k === 'e') this.useCellar()
      if (k === 'm') this.hud.toast(this.sound.toggle() ? 'Sound on' : 'Sound off', 'info', 1.5)
      if ([' ', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright', 'tab'].includes(k)) e.preventDefault()
    })
    on('keyup', e => this.keys.delete(e.key.toLowerCase()))
    on('blur', () => { this.keys.clear(); this.mouse.down = false; if (!this.over) this.setPaused(true) })
    const canvas = this.renderer.domElement
    const move = (e: MouseEvent) => { this.mouse.x = e.clientX; this.mouse.y = e.clientY; this.hud.crosshair(e.clientX, e.clientY) }
    on('mousemove', move)
    canvas.addEventListener('mousedown', e => { if (e.button === 0) { this.mouse.down = true; this.sound.unlock(); if (!this.player.auto) this.tryFire() } })
    on('mouseup', e => { if (e.button === 0) this.mouse.down = false })
    canvas.addEventListener('contextmenu', e => e.preventDefault())
  }

  private resize(): void {
    const w = this.root.clientWidth || window.innerWidth, h = this.root.clientHeight || window.innerHeight
    this.renderer.setSize(w, h)
    this.camera.aspect = w / h
    this.camera.updateProjectionMatrix()
  }

  private setPaused(p: boolean): void {
    if (this.over) return
    this.paused = p
    this.keys.clear(); this.mouse.down = false
    if (p) this.hud.screen('pause', { mission: this.mission }); else { this.hud.screen(null); this.clock.getDelta() }
  }

  dispose(): void {
    cancelAnimationFrame(this.raf)
    this.disposers.forEach(d => d())
    this.renderer.dispose()
    this.root.innerHTML = ''
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
        this.waveT = T.waveEvery
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
    if (m.state === 'E03_ASSAULT_FAILED') this.endScreen('failed')
    if (m.runner.outcome === 'SUCCESS') this.endScreen('success')
  }

  private applyModifiers(mods: SurpriseModifiers, event: string | undefined, announce: boolean): void {
    this.mods = mods
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
    if (blocked && !b.mesh.visible) { this.world.collision.add(b.box); b.mesh.visible = true }
    if (!blocked && b.mesh.visible) { this.world.collision.remove(b.box); b.mesh.visible = false }
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

  // ---------- enemies ----------

  private spawnEnemy(x: number, z: number, anchor: { x: number; z: number }): Enemy | undefined {
    if (this.enemies.filter(e => e.alive).length >= T.enemyCap) return undefined
    const p = { x, z }
    this.world.collision.resolve(p, 1.1)
    const mesh = this.soldier(0x7a4a3a, 0x3d2a22, 0xff5e63)
    mesh.position.set(p.x, 0, p.z)
    this.scene.add(mesh)
    const e: Enemy = { id: `hostile-${++this.enemySeq}`, x: p.x, z: p.z, hp: T.enemyHp, angle: 0, anchor, cooldown: 1 + Math.random(), sees: false, lastSeen: -99, lastKnown: null, wander: { ...p }, wanderT: 0, seenFor: 0, mesh, alive: true }
    this.enemies.push(e)
    return e
  }

  private garrison(phase: number): void {
    // Stragglers from earlier phases far from the player stand down.
    for (const e of this.enemies) if (e.alive && Math.hypot(e.x - this.player.x, e.z - this.player.z) > 70) this.removeEnemy(e)
    const open = this.mission.openObjectives()
    for (const id of open) {
      const o = OBJECTIVES[id]; if (!o) continue
      for (let i = 0; i < (GARRISON[phase] ?? 4); i++) {
        const a = Math.random() * Math.PI * 2, r = 6 + Math.random() * 18
        this.spawnEnemy(o.x + Math.cos(a) * r, o.z + Math.sin(a) * r, { x: o.x, z: o.z })
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
      this.spawnEnemy(target.x + Math.cos(ang) * 60, target.z + Math.sin(ang) * 60, target)
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

  private removeEnemy(e: Enemy): void {
    e.alive = false
    this.scene.remove(e.mesh)
    this.contacts.loseTracking(e.id, this.time * 1000)
  }

  private killEnemy(e: Enemy): void {
    if (!e.alive) return
    this.removeEnemy(e)
    this.stats.kills++
    this.puff(e.x, e.z, 0x5b3a2c, 3)
  }

  private updateEnemies(dt: number): void {
    const p = this.player, col = this.world.collision
    const strength = this.mission.counterattackStrength
    const detect = Math.max(24, T.detect * this.mods.visibilityScale)
    const sense = (this.sensorT -= dt) <= 0
    if (sense) this.sensorT = 0.2
    for (const e of this.enemies) {
      if (!e.alive) continue
      const dx = p.x - e.x, dz = p.z - e.z, dist = Math.hypot(dx, dz)
      if (sense) {
        e.sees = p.alive && dist < detect && col.clear(e.x, e.z, p.x, p.z)
        if (e.sees) { e.lastSeen = this.time; e.lastKnown = { x: p.x, z: p.z } }
        // Player observation feeds the contact picture Aegis validates against.
        const observed = p.alive && dist < 75 * this.mods.visibilityScale + 10 && col.clear(p.x, p.z, e.x, e.z)
        if (observed) {
          this.contacts.upsert({ id: e.id, classification: 'GAME_UNIT', position: toData(e.x, e.z), source: 'PLAYER', time: this.time * 1000, quality: 0.9 })
          e.seenFor += 0.2
          if (e.seenFor >= 1) this.contacts.confirm(e.id)
        } else if (this.contacts.get(e.id)?.currentlyObserved) { this.contacts.loseTracking(e.id, this.time * 1000); e.seenFor = 0 }
      }
      let mx = 0, mz = 0, speed = T.enemySpeed
      const leash = Math.hypot(e.x - e.anchor.x, e.z - e.anchor.z)
      if (e.sees) {
        e.angle = Math.atan2(dx, dz)
        if (dist > 30 && leash < 50) { mx = dx / dist; mz = dz / dist }
        else if (dist < 14) { mx = -dx / dist; mz = -dz / dist }
        const s = Math.sin(this.time * 1.3 + e.mesh.id) * 0.6
        mx += (-dz / dist) * s; mz += (dx / dist) * s
        e.cooldown -= dt
        if (e.cooldown <= 0) {
          e.cooldown = (0.85 + Math.random() * 0.7) / strength
          const spread = (0.07 + Math.random() * 0.05) * (2 - this.mods.visibilityScale)
          this.fire(e.x, e.z, e.angle + (Math.random() - 0.5) * spread * 2, false)
        }
      } else if (e.lastKnown && this.time - e.lastSeen < 4 && leash < 55) {
        const kx = e.lastKnown.x - e.x, kz = e.lastKnown.z - e.z, kd = Math.hypot(kx, kz)
        if (kd > 3) { mx = kx / kd; mz = kz / kd; e.angle = Math.atan2(kx, kz) }
      } else {
        e.wanderT -= dt
        if (e.wanderT <= 0) {
          const a = Math.random() * Math.PI * 2, r = Math.random() * 16
          e.wander = { x: e.anchor.x + Math.cos(a) * r, z: e.anchor.z + Math.sin(a) * r }
          e.wanderT = 4 + Math.random() * 4
        }
        const wx = e.wander.x - e.x, wz = e.wander.z - e.z, wd = Math.hypot(wx, wz)
        if (wd > 2) { mx = wx / wd; mz = wz / wd; e.angle = Math.atan2(wx, wz); speed = leash > 25 ? T.enemySpeed * 1.1 : 3 }
      }
      const ml = Math.hypot(mx, mz)
      if (ml > 0) { e.x += (mx / ml) * speed * dt; e.z += (mz / ml) * speed * dt }
      const pos = { x: e.x, z: e.z }; col.resolve(pos, 1.1); e.x = pos.x; e.z = pos.z
      e.mesh.position.set(e.x, 0, e.z)
      e.mesh.rotation.y = e.angle + Math.PI
    }
    if (this.enemies.length > 80) this.enemies = this.enemies.filter(e => e.alive)
  }

  // ---------- player ----------

  private updatePlayer(dt: number): void {
    const p = this.player
    if (!p.alive) {
      p.respawn -= dt
      this.hud.respawn(Math.max(0, p.respawn))
      if (p.respawn <= 0) this.respawn()
      return
    }
    let mx = 0, mz = 0
    if (this.keys.has('w') || this.keys.has('arrowup')) mz -= 1
    if (this.keys.has('s') || this.keys.has('arrowdown')) mz += 1
    if (this.keys.has('a') || this.keys.has('arrowleft')) mx -= 1
    if (this.keys.has('d') || this.keys.has('arrowright')) mx += 1
    const ml = Math.hypot(mx, mz)
    const speed = this.keys.has('shift') ? T.sprintSpeed : T.playerSpeed
    if (ml > 0) { p.x += (mx / ml) * speed * dt; p.z += (mz / ml) * speed * dt }
    const pos = { x: p.x, z: p.z }; this.world.collision.resolve(pos, 1.0); p.x = pos.x; p.z = pos.z

    // Aim at the ground point under the cursor.
    const rect = this.renderer.domElement.getBoundingClientRect()
    const ndc = new THREE.Vector2(((this.mouse.x - rect.left) / rect.width) * 2 - 1, -((this.mouse.y - rect.top) / rect.height) * 2 + 1)
    const ray = new THREE.Raycaster(); ray.setFromCamera(ndc, this.camera)
    ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 1, 0), -1.4), this.mouse.ground)
    p.angle = Math.atan2(this.mouse.ground.x - p.x, this.mouse.ground.z - p.z)
    this.playerMesh.position.set(p.x, 0, p.z)
    this.playerMesh.rotation.y = p.angle + Math.PI

    p.fireCd -= dt
    if (p.reload > 0) { p.reload -= dt; if (p.reload <= 0) { p.mag = this.mag; this.sound.click() } }
    if (this.mouse.down && p.auto) this.tryFire()
    if (this.time - p.lastHit > T.regenDelay) p.hp = Math.min(T.playerHp, p.hp + T.regenRate * dt)
  }

  private tryFire(): void {
    const p = this.player
    if (!p.alive || p.reload > 0 || p.fireCd > 0 || this.paused) return
    if (p.mag <= 0) { this.startReload(); return }
    p.mag--
    p.fireCd = p.auto ? T.fireInterval : 0.16
    this.fire(p.x, p.z, p.angle + (Math.random() - 0.5) * T.playerSpread * 2, true)
    this.sound.shot(1)
    if (p.mag === 0) this.startReload()
  }

  private startReload(): void {
    const p = this.player
    if (p.reload > 0 || p.mag === this.mag) return
    p.reload = T.reloadTime
  }

  private damagePlayer(amount: number): void {
    const p = this.player
    if (!p.alive) return
    p.hp -= amount
    p.lastHit = this.time
    this.hud.hit()
    if (p.hp > 0) return
    p.alive = false
    p.respawn = T.respawnDelay
    this.stats.deaths++
    this.playerMesh.visible = false
    this.puff(p.x, p.z, 0x4f6b45, 3)
    this.mission.spendReinforcements(1)
    this.sync()
  }

  /** Forward spawn at the most recently captured objective, else South Gate. */
  private respawn(): void {
    const p = this.player
    const last = this.mission.capturedObjectives.at(-1)
    const o = last ? OBJECTIVES[last] : undefined
    const at = o ? { x: o.x, z: o.z + 16 } : { ...SOUTH_GATE }
    this.world.collision.resolve(at, 1.0)
    Object.assign(p, { x: at.x, z: at.z, hp: T.playerHp, mag: this.mag, reload: 0, alive: true, lastHit: -99 })
    this.playerMesh.visible = true
    this.hud.respawn(null)
  }

  // ---------- shots ----------

  private fire(x: number, z: number, angle: number, player: boolean): void {
    const speed = player ? T.bulletSpeed : T.enemyBulletSpeed
    const mesh = new THREE.Mesh(BULLET_GEO, player ? BULLET_PLAYER : BULLET_ENEMY)
    const sx = Math.sin(angle), sz = Math.cos(angle)
    const b: Bullet = { x: x + sx * 1.8, z: z + sz * 1.8, vx: sx * speed, vz: sz * speed, life: player ? 1.1 : 1.4, damage: player ? T.playerDamage : T.enemyDamage, player, mesh }
    mesh.position.set(b.x, 1.6, b.z)
    mesh.rotation.y = angle
    this.scene.add(mesh)
    this.bullets.push(b)
    if (!player) {
      const d = Math.hypot(x - this.player.x, z - this.player.z)
      if (d < 90) this.sound.shot(Math.max(0.08, 0.5 - d / 180))
    }
  }

  private updateBullets(dt: number): void {
    const col = this.world.collision, p = this.player
    for (const b of this.bullets) {
      const nx = b.x + b.vx * dt, nz = b.z + b.vz * dt
      const t = col.segment(b.x, b.z, nx, nz)
      let hitT = t, hitEnemy: Enemy | undefined, hitPlayer = false
      if (b.player) {
        for (const e of this.enemies) {
          if (!e.alive) continue
          const tt = segCircle(b.x, b.z, nx, nz, e.x, e.z, 1.3)
          if (tt !== null && tt < hitT) { hitT = tt; hitEnemy = e }
        }
      } else if (p.alive) {
        const tt = segCircle(b.x, b.z, nx, nz, p.x, p.z, 1.0)
        if (tt !== null && tt < hitT) { hitT = tt; hitPlayer = true }
      }
      if (hitT < 1) {
        const hx = b.x + (nx - b.x) * hitT, hz = b.z + (nz - b.z) * hitT
        b.life = 0
        if (hitEnemy) {
          hitEnemy.hp -= b.damage
          hitEnemy.lastSeen = this.time; hitEnemy.lastKnown = { x: p.x, z: p.z }
          this.hud.hitmarker()
          if (hitEnemy.hp <= 0) this.killEnemy(hitEnemy)
        } else if (hitPlayer) this.damagePlayer(b.damage)
        else this.puff(hx, hz, 0xcbb894, 0.6)
      } else { b.x = nx; b.z = nz; b.life -= dt }
      b.mesh.position.set(b.x, 1.6, b.z)
    }
    for (const b of this.bullets) if (b.life <= 0) this.scene.remove(b.mesh)
    this.bullets = this.bullets.filter(b => b.life > 0)
  }

  private puff(x: number, z: number, color: number, size: number): void {
    const mesh = new THREE.Mesh(PUFF_GEO, new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.8 }))
    mesh.position.set(x, 1.4, z); mesh.scale.setScalar(size * 0.4)
    this.scene.add(mesh)
    this.fx.push({ mesh, life: 0.45, max: 0.45, grow: size })
  }

  private updateFx(dt: number): void {
    for (const f of this.fx) {
      f.life -= dt
      const k = 1 - f.life / f.max
      f.mesh.scale.setScalar(f.grow * (0.4 + k))
      const mat = (f.mesh as THREE.Mesh).material as THREE.MeshBasicMaterial
      mat.opacity = 0.8 * (1 - k)
      if (f.life <= 0) { this.scene.remove(f.mesh); mat.dispose() }
    }
    this.fx = this.fx.filter(f => f.life > 0)
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
      if (inPlayer && !inEnemy) v += dt / T.captureTime
      else if (!inPlayer && inEnemy) v -= dt / T.decayTime
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
    const mesh = new THREE.Group()
    const body = new THREE.Mesh(new THREE.BoxGeometry(3, 2.4, 9), new THREE.MeshLambertMaterial({ color: 0x3f4a3c }))
    const tail = new THREE.Mesh(new THREE.BoxGeometry(0.8, 0.8, 8), new THREE.MeshLambertMaterial({ color: 0x3f4a3c })); tail.position.z = 8
    const rotor = new THREE.Mesh(new THREE.BoxGeometry(18, 0.2, 0.8), new THREE.MeshLambertMaterial({ color: 0x1a1a1a })); rotor.position.y = 1.6; rotor.name = 'rotor'
    mesh.add(body, tail, rotor)
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
      for (let i = 0; i < 10; i++) this.puff(h.target.x + (Math.random() - 0.5) * T.falconRadius * 1.6, h.target.z + (Math.random() - 0.5) * T.falconRadius * 1.6, 0x4a4036, 4)
      for (const e of this.enemies) if (e.alive && Math.hypot(e.x - h.target.x, e.z - h.target.z) < T.falconRadius) this.killEnemy(e)
      this.sound.boom()
    }
    if (k >= 1) { this.scene.remove(h.mesh); this.falcon.heli = null; this.falcon.cd = T.falconCooldown }
  }

  private callAegis(): void {
    const aegis = this.mission.aegis
    if (aegis.state === 'OFFLINE') return this.hud.toast('Aegis offline: take the Mountain Fort.', 'warn', 3)
    if (this.aegisReq.pending) return
    if (this.aegisReq.cd > 0) return this.hud.toast(`Aegis recharging: ${Math.ceil(this.aegisReq.cd)}s`, 'info', 2)
    let target: Enemy | undefined, bd = 10
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
    this.aegisReq.pending = { id: target.id, t: T.aegisDelay, marker }
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
    this.scene.add(beam)
    this.fx.push({ mesh: beam, life: 0.6, max: 0.6, grow: 1 })
    this.killEnemy(e)
    this.sound.boom()
    this.aegisReq.cd = T.aegisCooldown
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

  // ---------- checkpoint / end ----------

  private restoreCheckpoint(): void {
    if (!this.mission.runner.restoreCheckpoint()) return
    for (const e of this.enemies) if (e.alive) this.removeEnemy(e)
    this.enemies = []
    for (const b of this.bullets) this.scene.remove(b.mesh)
    this.bullets = []
    if (this.aegisReq.pending) { this.scene.remove(this.aegisReq.pending.marker); this.aegisReq.pending = null }
    if (this.falcon.heli) { this.scene.remove(this.falcon.heli.mesh); this.falcon.heli = null }
    this.progress.clear()
    this.lastPhase = undefined
    this.lastUnlocks = new Set(this.mission.unlocks)
    this.lastSurprise = FORCE
    this.over = false
    this.paused = false
    this.hud.screen(null)
    this.sync()
    this.respawn()
    this.hud.toast('Restored from checkpoint', 'good', 2.5)
    this.clock.getDelta()
  }

  private endScreen(kind: 'failed' | 'success'): void {
    if (this.over) return
    this.over = true
    this.mouse.down = false
    const minutes = (performance.now() - this.stats.start) / 60000
    this.hud.screen(kind, { mission: this.mission, kills: this.stats.kills, deaths: this.stats.deaths, minutes })
  }

  // ---------- loop ----------

  private loop = (): void => {
    this.raf = requestAnimationFrame(this.loop)
    const dt = Math.min(0.05, this.clock.getDelta())
    if (!this.paused && !this.over) {
      this.time += dt
      this.updatePlayer(dt)
      this.updateEnemies(dt)
      this.updateBullets(dt)
      this.updateObjectives(dt)
      this.updateFalcon(dt)
      this.updateAegis(dt)
      if ((this.waveT -= dt) <= 0) { this.waveT = T.waveEvery; if (this.mission.phase !== undefined) this.wave() }
    }
    this.updateFx(dt)

    // Camera: angled overhead, looking north.
    const p = this.player
    const want = new THREE.Vector3(p.x, 82, p.z + 50)
    this.camera.position.lerp(want, 1 - Math.pow(0.001, dt))
    this.camera.lookAt(this.camera.position.x, 0, this.camera.position.z - 60)
    this.torch.position.set(p.x, 18, p.z)
    this.torch.target.position.set(p.x + Math.sin(p.angle) * 30, 0, p.z + Math.cos(p.angle) * 30)
    if (this.sand.visible) { this.sand.position.set(p.x, 0, p.z); this.sand.rotation.y += dt * 0.6 }

    this.renderer.render(this.scene, this.camera)
    this.hud.frame(this.frame())
  }

  private frame(): HudFrame {
    const m = this.mission, p = this.player
    const aegis = m.aegis.state
    return {
      phase: m.phase, objective: m.runner.objective,
      objectives: (STRONGHOLD.phases.find(x => x.order === m.phase)?.objectives ?? []).map(id => ({
        id, label: OBJECTIVES[id]?.label ?? id, done: m.capturedObjectives.includes(id), progress: this.progress.get(id) ?? 0
      })),
      anyOne: m.phase !== undefined && EPISODE.phaseRules[String(m.phase)] === 'any',
      reinforcements: m.reinforcements, hp: Math.max(0, p.hp) / T.playerHp, mag: p.mag, magSize: this.mag, reloading: p.reload > 0, auto: p.auto,
      falcon: !m.unlocks.includes('falcon_air_support') ? 'LOCKED' : !m.airSupportAvailable ? 'NO CREW' : this.falcon.heli ? 'INBOUND' : this.falcon.cd > 0 ? `${Math.ceil(this.falcon.cd)}s` : 'READY',
      aegis: aegis === 'OFFLINE' ? 'OFFLINE' : this.aegisReq.pending ? 'AUTHORISED' : this.aegisReq.cd > 0 ? `${Math.ceil(this.aegisReq.cd)}s` : 'READY',
      surprise: m.surprise.active ?? null, plannedSurprise: m.surprise.planned,
      map: {
        player: { x: p.x, z: p.z, angle: p.angle },
        enemies: this.enemies.filter(e => e.alive).map(e => ({ x: e.x, z: e.z, confirmed: this.contacts.get(e.id)?.state === 'CONFIRMED' })),
        reveal: 120 * this.mods.droneSensorRangeScale,
        interference: this.mods.relayInterference,
        objectives: Object.entries(OBJECTIVES).map(([id, o]) => ({ x: o.x, z: o.z, state: m.capturedObjectives.includes(id) ? 'done' as const : m.openObjectives().includes(id) ? 'open' as const : 'locked' as const })),
        cellars: this.mods.openedRoutes.includes('old_gardun_cellars'),
        bridgeBlocked: this.mods.blockedRoutes.includes('riverbed_bridge')
      }
    }
  }
}

const BULLET_GEO = new THREE.BoxGeometry(0.18, 0.18, 2.6)
const BULLET_PLAYER = new THREE.MeshBasicMaterial({ color: 0xfff2b0 })
const BULLET_ENEMY = new THREE.MeshBasicMaterial({ color: 0xff7a5c })
const PUFF_GEO = new THREE.SphereGeometry(1, 8, 6)

/** Fraction along a→b where it first enters the circle, or null. */
function segCircle(ax: number, az: number, bx: number, bz: number, cx: number, cz: number, r: number): number | null {
  const dx = bx - ax, dz = bz - az, fx = ax - cx, fz = az - cz
  const a = dx * dx + dz * dz, b = 2 * (fx * dx + fz * dz), c = fx * fx + fz * fz - r * r
  if (c <= 0) return 0
  const disc = b * b - 4 * a * c
  if (disc < 0 || a === 0) return null
  const t = (-b - Math.sqrt(disc)) / (2 * a)
  return t >= 0 && t <= 1 ? t : null
}
