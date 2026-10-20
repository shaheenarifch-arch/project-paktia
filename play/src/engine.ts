import * as THREE from 'three'
import kr47Json from '../../data/weapons/kr47.json'
import type { Collision } from './collision'
import { Hud, type HudFrame, type CardSpec } from './hud'
import { Sound } from './sound'

// Shared battle layer for every episode: the player and KR-47, hostile
// behaviour, shots, effects, camera, input, pause and end screens. Each
// episode subclasses Battle, builds its map and reports results into its
// mission from the domain layer (src/), which owns the rules.

export const T = {
  playerSpeed: 15, sprintSpeed: 23, playerHp: 100, regenDelay: 4, regenRate: 14,
  fireInterval: 0.095, reloadTime: 1.8, playerDamage: 22, playerSpread: 0.025, bulletSpeed: 190,
  enemyHp: 60, enemySpeed: 5.5, enemyDamage: 7, enemyBulletSpeed: 95, detect: 54, enemyCap: 30
}

/** Anything hostiles can shoot at: the player, convoy vehicles. */
export interface Friendly { id: string; x: number; z: number; r: number; alive: boolean; hit(damage: number): void }

export interface Hostile {
  id: string; tag: string; x: number; z: number; hp: number; angle: number; anchor: { x: number; z: number }; leash: number
  cooldown: number; sees: boolean; lastSeen: number; lastKnown: { x: number; z: number } | null; aim: Friendly | null
  wander: { x: number; z: number }; wanderT: number; seenFor: number; mesh: THREE.Group; alive: boolean
}
interface Bullet { x: number; z: number; vx: number; vz: number; life: number; damage: number; friendly: boolean; mesh: THREE.Mesh }
interface Fx { mesh: THREE.Object3D; life: number; max: number; grow: number }

export interface BattleOptions { onExit: () => void }

export abstract class Battle {
  protected readonly scene = new THREE.Scene()
  protected readonly camera = new THREE.PerspectiveCamera(50, 1, 0.5, 1200)
  protected readonly renderer: THREE.WebGLRenderer
  protected readonly hud: Hud
  protected readonly sound = new Sound()
  protected readonly clock = new THREE.Clock()
  protected readonly sun: THREE.DirectionalLight
  protected readonly hemi: THREE.HemisphereLight
  protected readonly torch: THREE.SpotLight
  protected collision!: Collision

  protected player = { x: 0, z: 0, hp: T.playerHp, angle: Math.PI, mag: 30, reload: 0, fireCd: 0, lastHit: -99, alive: true, respawn: 0, auto: true }
  protected readonly playerMesh: THREE.Group
  protected readonly playerFriendly: Friendly
  protected enemies: Hostile[] = []
  protected bullets: Bullet[] = []
  protected fx: Fx[] = []
  protected keys = new Set<string>()
  protected mouse = { x: 0, y: 0, down: false, ground: new THREE.Vector3() }
  protected time = 0
  protected paused = false
  protected over = false
  protected stats = { kills: 0, deaths: 0, start: 0 }
  /** Scales hostile detection and accuracy (surprise events lower it). */
  protected visibility = 1
  /** Seconds before a downed player is back. */
  protected respawnDelay = 3
  protected readonly mag: number
  private enemySeq = 0
  private raf = 0
  private readonly disposers: (() => void)[] = []

  constructor(protected readonly root: HTMLElement, protected readonly opts: BattleOptions) {
    this.mag = kr47Json.magazineCapacity
    this.player.mag = this.mag
    this.player.auto = kr47Json.fireModes.includes('automatic')

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

    this.playerMesh = soldierMesh(0x4f6b45, 0x2f3d2a, 0x78ffab)
    this.scene.add(this.playerMesh)
    const self = this
    this.playerFriendly = {
      id: 'player', r: 1.0,
      get x() { return self.player.x }, get z() { return self.player.z }, get alive() { return self.player.alive },
      hit: (d: number) => this.damagePlayer(d)
    }

    this.hud = new Hud(root)
  }

  /** Subclasses call this once their map is built. */
  protected start(): void {
    this.bindInput()
    this.resize()
    this.stats.start = performance.now()
    this.loop()
    // Dev-server only (removed from production builds): lets automated playtests drive the game.
    if (import.meta.env.DEV) (window as unknown as Record<string, unknown>).__paktia = this
  }

  // ---------- hooks for episodes ----------

  /** Mission logic each frame, after the player, hostiles and shots have moved. */
  protected abstract update(dt: number): void
  protected abstract hudFrame(): HudFrame
  /** Where a downed player comes back. */
  protected abstract respawnPoint(): { x: number; z: number }
  /** Restores the mission's last checkpoint and resets the map to match. */
  protected abstract restoreCheckpoint(): void
  protected onKey(_key: string): void {}
  protected onPlayerDown(): void {}
  protected onHostileDown(_e: Hostile): void {}
  /** Everything hostiles may target. */
  protected friendlies(): Friendly[] { return [this.playerFriendly] }
  /** What the camera follows. */
  protected focus(): { x: number; z: number } { return this.player }
  /** True while WASD steers something other than the soldier (a drone). */
  protected steeringOther(): boolean { return false }
  protected cameraHeight(): number { return 82 }

  // ---------- input ----------

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
      if (k === 'm') this.hud.toast(this.sound.toggle() ? 'Sound on' : 'Sound off', 'info', 1.5)
      this.onKey(k)
      if ([' ', 'arrowup', 'arrowdown', 'arrowleft', 'arrowright', 'tab'].includes(k)) e.preventDefault()
    })
    on('keyup', e => this.keys.delete(e.key.toLowerCase()))
    on('blur', () => { this.keys.clear(); this.mouse.down = false; if (!this.over) this.setPaused(true) })
    on('mousemove', (e: MouseEvent) => { this.mouse.x = e.clientX; this.mouse.y = e.clientY; this.hud.crosshair(e.clientX, e.clientY) })
    const canvas = this.renderer.domElement
    canvas.addEventListener('mousedown', e => { if (e.button === 0) { this.mouse.down = true; this.sound.unlock(); if (!this.player.auto) this.tryFire() } })
    on('mouseup', (e: MouseEvent) => { if (e.button === 0) this.mouse.down = false })
    canvas.addEventListener('contextmenu', e => e.preventDefault())
  }

  private resize(): void {
    const w = this.root.clientWidth || window.innerWidth, h = this.root.clientHeight || window.innerHeight
    this.renderer.setSize(w, h)
    this.camera.aspect = w / h
    this.camera.updateProjectionMatrix()
  }

  protected setPaused(p: boolean): void {
    if (this.over) return
    this.paused = p
    this.keys.clear(); this.mouse.down = false
    if (p) this.hud.card(this.pauseCard()); else { this.hud.card(null); this.clock.getDelta() }
  }

  protected pauseCard(): CardSpec {
    return {
      eyebrow: 'PAUSED', title: 'Paused', text: '', controls: true,
      buttons: [{ label: 'Resume', primary: true, action: () => this.setPaused(false) }, { label: 'Quit to menu', action: () => this.exit() }]
    }
  }

  protected exit(): void { this.dispose(); this.opts.onExit() }

  dispose(): void {
    cancelAnimationFrame(this.raf)
    this.disposers.forEach(d => d())
    this.renderer.dispose()
    this.root.innerHTML = ''
  }

  // ---------- hostiles ----------

  protected spawnHostile(x: number, z: number, opts: { tag?: string; anchor?: { x: number; z: number }; leash?: number; id?: string; force?: boolean } = {}): Hostile | undefined {
    if (!opts.force && this.enemies.filter(e => e.alive).length >= T.enemyCap) return undefined
    const p = { x, z }
    this.collision.resolve(p, 1.1)
    const mesh = soldierMesh(0x7a4a3a, 0x3d2a22, 0xff5e63)
    mesh.position.set(p.x, 0, p.z)
    this.scene.add(mesh)
    const e: Hostile = {
      id: opts.id ?? `hostile-${++this.enemySeq}`, tag: opts.tag ?? '', x: p.x, z: p.z, hp: T.enemyHp, angle: 0, anchor: opts.anchor ?? { ...p }, leash: opts.leash ?? 50,
      cooldown: 1 + Math.random(), sees: false, lastSeen: -99, lastKnown: null, aim: null, wander: { ...p }, wanderT: 0, seenFor: 0, mesh, alive: true
    }
    this.enemies.push(e)
    return e
  }

  protected removeHostile(e: Hostile): void {
    e.alive = false
    this.scene.remove(e.mesh)
  }

  protected killHostile(e: Hostile): void {
    if (!e.alive) return
    this.removeHostile(e)
    this.stats.kills++
    this.puff(e.x, e.z, 0x5b3a2c, 3)
    this.onHostileDown(e)
  }

  protected clearHostiles(): void {
    for (const e of this.enemies) if (e.alive) this.removeHostile(e)
    this.enemies = []
  }

  protected alive(tag?: string): Hostile[] { return this.enemies.filter(e => e.alive && (tag === undefined || e.tag === tag)) }

  private sensorT = 0
  private updateHostiles(dt: number): void {
    const col = this.collision
    const detect = Math.max(24, T.detect * this.visibility)
    const sense = (this.sensorT -= dt) <= 0
    if (sense) this.sensorT = 0.2
    const targets = this.friendlies().filter(f => f.alive)
    for (const e of this.enemies) {
      if (!e.alive) continue
      if (sense) {
        // Nearest friendly in sight.
        let best: Friendly | null = null, bd = detect
        for (const f of targets) {
          const d = Math.hypot(f.x - e.x, f.z - e.z)
          if (d < bd && col.clear(e.x, e.z, f.x, f.z)) { bd = d; best = f }
        }
        e.aim = best
        e.sees = Boolean(best)
        if (best) { e.lastSeen = this.time; e.lastKnown = { x: best.x, z: best.z } }
      }
      let mx = 0, mz = 0, speed = T.enemySpeed
      const leash = Math.hypot(e.x - e.anchor.x, e.z - e.anchor.z)
      const t = e.aim
      if (e.sees && t && t.alive) {
        const dx = t.x - e.x, dz = t.z - e.z, dist = Math.hypot(dx, dz) || 1
        e.angle = Math.atan2(dx, dz)
        if (dist > 30 && leash < e.leash) { mx = dx / dist; mz = dz / dist }
        else if (dist < 14) { mx = -dx / dist; mz = -dz / dist }
        const s = Math.sin(this.time * 1.3 + e.mesh.id) * 0.6
        mx += (-dz / dist) * s; mz += (dx / dist) * s
        e.cooldown -= dt
        if (e.cooldown <= 0) {
          e.cooldown = (0.85 + Math.random() * 0.7) / this.hostileFireRate()
          const spread = (0.07 + Math.random() * 0.05) * (2 - this.visibility)
          this.fire(e.x, e.z, e.angle + (Math.random() - 0.5) * spread * 2, false)
        }
      } else if (e.lastKnown && this.time - e.lastSeen < 4 && leash < e.leash + 5) {
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
      const pos = { x: e.x, z: e.z }; col.resolve(pos, 1.1); this.pushOut(pos, 1.1); e.x = pos.x; e.z = pos.z
      e.mesh.position.set(e.x, 0, e.z)
      e.mesh.rotation.y = e.angle + Math.PI
    }
    if (this.enemies.length > 80) this.enemies = this.enemies.filter(e => e.alive)
  }

  /** Hostile rate of fire multiplier (Episode 03 lowers it once enemy comms are down). */
  protected hostileFireRate(): number { return 1 }

  /** Keeps a moving circle out of non-map obstacles such as vehicles. */
  protected pushOut(_p: { x: number; z: number }, _r: number): void {}

  // ---------- player ----------

  private updatePlayer(dt: number): void {
    const p = this.player
    if (!p.alive) {
      p.respawn -= dt
      this.hud.respawn(Math.max(0, p.respawn))
      if (p.respawn <= 0) this.respawn()
      return
    }
    if (!this.steeringOther()) {
      const [mx, mz] = this.moveInput()
      const ml = Math.hypot(mx, mz)
      const speed = this.keys.has('shift') ? T.sprintSpeed : T.playerSpeed
      if (ml > 0) { p.x += (mx / ml) * speed * dt; p.z += (mz / ml) * speed * dt }
    }
    const pos = { x: p.x, z: p.z }; this.collision.resolve(pos, 1.0); this.pushOut(pos, 1.0); p.x = pos.x; p.z = pos.z

    // Aim at the ground point under the cursor.
    const rect = this.renderer.domElement.getBoundingClientRect()
    const ndc = new THREE.Vector2(((this.mouse.x - rect.left) / rect.width) * 2 - 1, -((this.mouse.y - rect.top) / rect.height) * 2 + 1)
    const ray = new THREE.Raycaster(); ray.setFromCamera(ndc, this.camera)
    ray.ray.intersectPlane(new THREE.Plane(new THREE.Vector3(0, 1, 0), -1.4), this.mouse.ground)
    if (!this.steeringOther()) p.angle = Math.atan2(this.mouse.ground.x - p.x, this.mouse.ground.z - p.z)
    this.playerMesh.position.set(p.x, 0, p.z)
    this.playerMesh.rotation.y = p.angle + Math.PI

    p.fireCd -= dt
    if (p.reload > 0) { p.reload -= dt; if (p.reload <= 0) { p.mag = this.mag; this.sound.click() } }
    if (this.mouse.down && p.auto && !this.steeringOther()) this.tryFire()
    if (this.time - p.lastHit > T.regenDelay) p.hp = Math.min(T.playerHp, p.hp + T.regenRate * dt)
  }

  protected moveInput(): [number, number] {
    let mx = 0, mz = 0
    if (this.keys.has('w') || this.keys.has('arrowup')) mz -= 1
    if (this.keys.has('s') || this.keys.has('arrowdown')) mz += 1
    if (this.keys.has('a') || this.keys.has('arrowleft')) mx -= 1
    if (this.keys.has('d') || this.keys.has('arrowright')) mx += 1
    return [mx, mz]
  }

  private tryFire(): void {
    const p = this.player
    if (!p.alive || p.reload > 0 || p.fireCd > 0 || this.paused || this.steeringOther()) return
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

  protected damagePlayer(amount: number): void {
    const p = this.player
    if (!p.alive) return
    p.hp -= amount
    p.lastHit = this.time
    this.hud.hit()
    if (p.hp > 0) return
    p.alive = false
    p.respawn = this.respawnDelay
    this.stats.deaths++
    this.playerMesh.visible = false
    this.puff(p.x, p.z, 0x4f6b45, 3)
    this.onPlayerDown()
  }

  protected respawn(): void {
    const p = this.player
    const at = this.respawnPoint()
    this.collision.resolve(at, 1.0)
    Object.assign(p, { x: at.x, z: at.z, hp: T.playerHp, mag: this.mag, reload: 0, alive: true, lastHit: -99 })
    this.playerMesh.visible = true
    this.hud.respawn(null)
  }

  // ---------- shots and effects ----------

  protected fire(x: number, z: number, angle: number, friendly: boolean, damage?: number): void {
    const speed = friendly ? T.bulletSpeed : T.enemyBulletSpeed
    const mesh = new THREE.Mesh(BULLET_GEO, friendly ? BULLET_FRIENDLY : BULLET_HOSTILE)
    const sx = Math.sin(angle), sz = Math.cos(angle)
    const b: Bullet = { x: x + sx * 1.8, z: z + sz * 1.8, vx: sx * speed, vz: sz * speed, life: friendly ? 1.1 : 1.4, damage: damage ?? (friendly ? T.playerDamage : T.enemyDamage), friendly, mesh }
    mesh.position.set(b.x, 1.6, b.z)
    mesh.rotation.y = angle
    this.scene.add(mesh)
    this.bullets.push(b)
    if (!friendly) {
      const d = Math.hypot(x - this.player.x, z - this.player.z)
      if (d < 90) this.sound.shot(Math.max(0.08, 0.5 - d / 180))
    }
  }

  private updateBullets(dt: number): void {
    const col = this.collision
    const friends = this.friendlies().filter(f => f.alive)
    for (const b of this.bullets) {
      const nx = b.x + b.vx * dt, nz = b.z + b.vz * dt
      let hitT = col.segment(b.x, b.z, nx, nz), hitEnemy: Hostile | undefined, hitFriend: Friendly | undefined
      if (b.friendly) {
        for (const e of this.enemies) {
          if (!e.alive) continue
          const tt = segCircle(b.x, b.z, nx, nz, e.x, e.z, 1.3)
          if (tt !== null && tt < hitT) { hitT = tt; hitEnemy = e }
        }
      } else {
        for (const f of friends) {
          const tt = segCircle(b.x, b.z, nx, nz, f.x, f.z, f.r)
          if (tt !== null && tt < hitT) { hitT = tt; hitFriend = f }
        }
      }
      if (hitT < 1) {
        const hx = b.x + (nx - b.x) * hitT, hz = b.z + (nz - b.z) * hitT
        b.life = 0
        if (hitEnemy) {
          hitEnemy.hp -= b.damage
          hitEnemy.lastSeen = this.time; hitEnemy.lastKnown = { x: this.player.x, z: this.player.z }
          this.hud.hitmarker()
          if (hitEnemy.hp <= 0) this.killHostile(hitEnemy)
        } else if (hitFriend) { hitFriend.hit(b.damage); if (hitFriend !== this.playerFriendly) this.puff(hx, hz, 0x3a3a3a, 0.8) }
        else this.puff(hx, hz, 0xcbb894, 0.6)
      } else { b.x = nx; b.z = nz; b.life -= dt }
      b.mesh.position.set(b.x, 1.6, b.z)
    }
    for (const b of this.bullets) if (b.life <= 0) this.scene.remove(b.mesh)
    this.bullets = this.bullets.filter(b => b.life > 0)
  }

  protected clearBullets(): void {
    for (const b of this.bullets) this.scene.remove(b.mesh)
    this.bullets = []
  }

  protected puff(x: number, z: number, color: number, size: number): void {
    const mesh = new THREE.Mesh(PUFF_GEO, new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 0.8 }))
    mesh.position.set(x, 1.4, z); mesh.scale.setScalar(size * 0.4)
    this.scene.add(mesh)
    this.fx.push({ mesh, life: 0.45, max: 0.45, grow: size })
  }

  protected flash(mesh: THREE.Object3D, seconds: number): void {
    this.scene.add(mesh)
    this.fx.push({ mesh, life: seconds, max: seconds, grow: 1 })
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

  // ---------- end ----------

  protected endScreen(card: CardSpec): void {
    if (this.over) return
    this.over = true
    this.mouse.down = false
    this.hud.card(card)
  }

  /** Clears the end screen after a checkpoint restore. */
  protected resume(): void {
    this.over = false
    this.paused = false
    this.hud.card(null)
    this.clock.getDelta()
  }

  protected statsBlock(): { label: string; value: string }[] {
    return [
      { label: 'hostiles neutralised', value: String(this.stats.kills) },
      { label: 'times down', value: String(this.stats.deaths) },
      { label: 'minutes', value: ((performance.now() - this.stats.start) / 60000).toFixed(1) }
    ]
  }

  // ---------- loop ----------

  private loop = (): void => {
    this.raf = requestAnimationFrame(this.loop)
    const dt = Math.min(0.05, this.clock.getDelta())
    if (!this.paused && !this.over) {
      this.time += dt
      this.updatePlayer(dt)
      this.updateHostiles(dt)
      this.updateBullets(dt)
      this.update(dt)
    }
    this.updateFx(dt)

    // Camera: angled overhead, looking north.
    const f = this.focus(), h = this.cameraHeight()
    const want = new THREE.Vector3(f.x, h, f.z + h * 0.61)
    this.camera.position.lerp(want, 1 - Math.pow(0.001, dt))
    this.camera.lookAt(this.camera.position.x, 0, this.camera.position.z - h * 0.73)
    const p = this.player
    this.torch.position.set(p.x, 18, p.z)
    this.torch.target.position.set(p.x + Math.sin(p.angle) * 30, 0, p.z + Math.cos(p.angle) * 30)

    this.renderer.render(this.scene, this.camera)
    this.hud.frame(this.hudFrame())
  }
}

export function soldierMesh(body: number, dark: number, ring?: number): THREE.Group {
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

const BULLET_GEO = new THREE.BoxGeometry(0.18, 0.18, 2.6)
const BULLET_FRIENDLY = new THREE.MeshBasicMaterial({ color: 0xfff2b0 })
const BULLET_HOSTILE = new THREE.MeshBasicMaterial({ color: 0xff7a5c })
const PUFF_GEO = new THREE.SphereGeometry(1, 8, 6)

/** Fraction along a→b where it first enters the circle, or null. */
export function segCircle(ax: number, az: number, bx: number, bz: number, cx: number, cz: number, r: number): number | null {
  const dx = bx - ax, dz = bz - az, fx = ax - cx, fz = az - cz
  const a = dx * dx + dz * dz, b = 2 * (fx * dx + fz * dz), c = fx * fx + fz * fz - r * r
  if (c <= 0) return 0
  const disc = b * b - 4 * a * c
  if (disc < 0 || a === 0) return null
  const t = (-b - Math.sqrt(disc)) / (2 * a)
  return t >= 0 && t <= 1 ? t : null
}
