import * as THREE from 'three'
import type { MissionRunner } from '../../src/index.js'
import { Battle, type BattleOptions, type Hostile } from './engine'
import { objectiveMarker } from './gardun'
import type { CardSpec, HudFrame, MapLine, MapMarker } from './hud'

// Shared layer for the objective-driven episodes (01 and 04): capture/hold
// sites, the Raven drone, choice cards, wave spawns and the standard HUD
// frame. Each episode owns its map and its mission; this only adds what both
// need on top of Battle. The rules stay in the domain layer (src/).

export interface SiteSpec { id: string; x: number; z: number; r: number; label: string; seconds: number }
interface Site extends SiteSpec { progress: number; marker: ReturnType<typeof objectiveMarker> }

export interface FrameSpec {
  eyebrow: string; title: string; phase: string
  items: HudFrame['items']; vitals: HudFrame['vitals']; abilities: HudFrame['abilities']
  status?: string | null; hint?: string | null
  bounds: { x0: number; z0: number; x1: number; z1: number }
  lines?: MapLine[]; markers?: MapMarker[]
}

export abstract class ScenarioBattle extends Battle {
  protected abstract readonly runner: MissionRunner
  protected sites = new Map<string, Site>()
  protected raven = { flying: false, x: 0, z: 0, speed: 46, mesh: new THREE.Group() }
  private choosing = false
  private keyPick: ((e: KeyboardEvent) => void) | undefined

  protected initRaven(): void {
    const body = new THREE.Mesh(new THREE.BoxGeometry(3, 0.8, 3), new THREE.MeshLambertMaterial({ color: 0x2c3a40 }))
    const rotor = new THREE.Mesh(new THREE.BoxGeometry(7, 0.15, 0.5), new THREE.MeshLambertMaterial({ color: 0x78ffab }))
    this.raven.mesh.add(body, rotor)
    this.raven.mesh.position.y = 14
    this.scene.add(this.raven.mesh)
  }

  // ---------- sites ----------

  protected addSite(spec: SiteSpec): void {
    if (this.sites.has(spec.id)) return
    const marker = objectiveMarker(spec.r)
    marker.group.position.set(spec.x, 0, spec.z)
    this.scene.add(marker.group)
    this.sites.set(spec.id, { ...spec, progress: 0, marker })
  }

  protected removeSite(id: string): void {
    const s = this.sites.get(id)
    if (!s) return
    this.scene.remove(s.marker.group)
    this.sites.delete(id)
  }

  protected clearSites(): void { for (const id of [...this.sites.keys()]) this.removeSite(id) }
  protected siteProgress(id: string): number { return this.sites.get(id)?.progress ?? 0 }

  /** Called once a site has been held long enough. */
  protected abstract onSite(id: string): void

  /** Hold a site with nobody hostile inside it to complete it. */
  protected updateSites(dt: number): void {
    for (const s of [...this.sites.values()]) {
      const p = this.player
      const inside = p.alive && !this.raven.flying && Math.hypot(p.x - s.x, p.z - s.z) < s.r
      const contested = this.enemies.some(e => e.alive && Math.hypot(e.x - s.x, e.z - s.z) < s.r * 1.15)
      if (inside && !contested) s.progress = Math.min(1, s.progress + dt / s.seconds)
      else s.progress = Math.max(0, s.progress - dt / (s.seconds * 3))
      s.marker.fill.scale.setScalar(Math.max(0.001, s.progress))
      ;(s.marker.ring.material as THREE.MeshBasicMaterial).color.setHex(contested && inside ? 0xff5e63 : 0xffd166)
      if (s.progress >= 1) {
        this.removeSite(s.id)
        this.sound.chime(true)
        this.onSite(s.id)
      }
    }
  }

  // ---------- Raven ----------

  protected override steeringOther(): boolean { return this.raven.flying }
  protected override focus(): { x: number; z: number } { return this.raven.flying ? this.raven : this.player }
  protected override cameraHeight(): number { return this.raven.flying ? 118 : 82 }
  protected canRaven(): boolean { return true }

  protected toggleRaven(): void {
    const r = this.raven
    if (!r.flying) {
      if (!this.player.alive || !this.canRaven()) { this.hud.toast('Raven is not available right now.', 'info', 2); return }
      r.flying = true
      this.hud.toast('Flying Raven. The squad holds position. Press Q to return.', 'info', 2.5)
    } else {
      r.flying = false
      this.hud.toast('Back with the squad.', 'info', 2)
    }
    this.onRavenToggle()
  }

  protected onRavenToggle(): void {}

  protected updateRaven(dt: number, bounds: { x0: number; z0: number; x1: number; z1: number }): void {
    const r = this.raven
    if (r.flying && !this.player.alive) { r.flying = false; this.onRavenToggle() }
    if (r.flying) {
      const [mx, mz] = this.moveInput(), ml = Math.hypot(mx, mz)
      if (ml > 0) {
        r.x = Math.max(bounds.x0 + 4, Math.min(bounds.x1 - 4, r.x + (mx / ml) * r.speed * dt))
        r.z = Math.max(bounds.z0 + 4, Math.min(bounds.z1 - 4, r.z + (mz / ml) * r.speed * dt))
      }
    } else {
      r.x += (this.player.x + 4 - r.x) * Math.min(1, dt * 3)
      r.z += (this.player.z + 4 - r.z) * Math.min(1, dt * 3)
    }
    r.mesh.position.set(r.x, 14, r.z)
    r.mesh.rotation.y += dt * 0.8
  }

  protected override onKey(key: string): void {
    if (key === 'q') this.toggleRaven()
  }

  // ---------- spawns ----------

  protected group(count: number, cx: number, cz: number, spread: number, anchor?: { x: number; z: number }, leash = 60, tag = ''): Hostile[] {
    const out: Hostile[] = []
    for (let i = 0; i < count; i++) {
      const a = Math.random() * Math.PI * 2, d = Math.random() * spread
      const e = this.spawnHostile(cx + Math.cos(a) * d, cz + Math.sin(a) * d, { anchor: anchor ?? { x: cx, z: cz }, leash, tag, force: true })
      if (e) out.push(e)
    }
    return out
  }

  protected countWithin(x: number, z: number, r: number): number {
    return this.enemies.filter(e => e.alive && Math.hypot(e.x - x, e.z - z) < r).length
  }

  // ---------- cards ----------

  /** A blocking choice card; keys 1..n also pick. */
  protected choose(eyebrow: string, title: string, text: string, options: { label: string; detail: string; pick: () => void }[]): void {
    if (this.choosing || this.over) return
    this.choosing = true
    this.paused = true
    this.keys.clear(); this.mouse.down = false
    const done = (fn: () => void) => {
      this.choosing = false; this.paused = false
      if (this.keyPick) window.removeEventListener('keydown', this.keyPick)
      this.keyPick = undefined
      this.hud.card(null); this.clock.getDelta(); fn()
    }
    this.hud.card({ eyebrow, title, text, options: options.map(o => ({ label: o.label, detail: o.detail, action: () => done(o.pick) })) })
    this.keyPick = (e: KeyboardEvent) => { const o = options[Number(e.key) - 1]; if (o) done(o.pick) }
    window.addEventListener('keydown', this.keyPick)
  }

  /** A blocking briefing card; the mission starts once it is dismissed. */
  protected briefing(spec: { eyebrow: string; title: string; text: string; button: string }, then: () => void): void {
    this.paused = true
    this.choosing = true
    const go = () => { this.choosing = false; this.paused = false; this.hud.card(null); this.clock.getDelta(); then() }
    this.hud.card({ eyebrow: spec.eyebrow, title: spec.title, text: spec.text, controls: true, buttons: [{ label: spec.button, primary: true, action: go }] })
  }

  protected override setPaused(p: boolean): void {
    if (this.choosing) return
    super.setPaused(p)
  }

  override dispose(): void {
    if (this.keyPick) window.removeEventListener('keydown', this.keyPick)
    super.dispose()
  }

  protected failCard(title: string, text: string): void {
    this.endScreen({
      eyebrow: 'OPERATION FAILED', eyebrowKind: 'warn', title, text, stats: this.statsBlock(),
      buttons: [{ label: 'Restore checkpoint', primary: true, action: () => this.restoreCheckpoint() }, { label: 'Quit to menu', action: () => this.exit() }]
    })
  }

  protected winCard(spec: { eyebrow: string; title: string; text: string; good: boolean; stats?: { label: string; value: string }[] }): void {
    const card: CardSpec = {
      eyebrow: spec.eyebrow, ...(spec.good ? { eyebrowKind: 'good' as const } : {}), title: spec.title, text: spec.text,
      stats: [...(spec.stats ?? []), ...this.statsBlock()],
      buttons: [{ label: 'Back to campaign', primary: true, action: () => this.exit() }]
    }
    this.endScreen(card)
  }

  /** Moves the mission along any state that has nothing left to wait for. */
  protected autoAdvance(): void {
    const r = this.runner
    if (this.choosing || r.complete || r.choices.length > 0 || r.missing().length > 0) return
    r.advance()
  }

  // ---------- hud ----------

  protected frameFor(f: FrameSpec): HudFrame {
    const p = this.player
    const from = this.raven.flying ? this.raven : p
    return {
      eyebrow: f.eyebrow, title: f.title, phase: f.phase, objective: this.runner.objective, items: f.items, vitals: f.vitals,
      hp: Math.max(0, p.hp) / 100, mag: p.mag, magSize: this.mag, reloading: p.reload > 0, auto: p.auto,
      abilities: f.abilities, status: f.status ?? null, hint: f.hint ?? null,
      map: {
        bounds: f.bounds,
        lines: f.lines ?? [],
        markers: [
          ...(f.markers ?? []),
          ...[...this.sites.values()].map(s => ({ x: s.x, z: s.z, color: '#ffd166', size: 4, shape: 'ring' as const }))
        ],
        player: { x: p.x, z: p.z, angle: p.angle },
        enemies: this.enemies.filter(e => e.alive).map(e => ({ x: e.x, z: e.z, confirmed: this.raven.flying })),
        reveal: this.raven.flying ? 150 : 95, revealFrom: { x: from.x, z: from.z }, interference: 0
      }
    }
  }
}

export type { BattleOptions }
