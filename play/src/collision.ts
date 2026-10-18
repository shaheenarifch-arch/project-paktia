import * as THREE from 'three'

export interface Bounds { x0: number; z0: number; x1: number; z1: number }
export interface Box extends Bounds { h: number; blocksShots: boolean; tag?: string }

/** Uniform grid so movement and bullets only test nearby boxes. */
export class Collision {
  private readonly cell = 24
  private readonly cells = new Map<number, Box[]>()
  readonly boxes: Box[] = []

  constructor(readonly bounds: Bounds) {}

  private key(cx: number, cz: number): number { return (cx + 500) * 2000 + (cz + 500) }
  private range(b: Bounds, fn: (k: number) => void): void {
    const c = this.cell
    for (let cx = Math.floor(b.x0 / c); cx <= Math.floor(b.x1 / c); cx++)
      for (let cz = Math.floor(b.z0 / c); cz <= Math.floor(b.z1 / c); cz++) fn(this.key(cx, cz))
  }

  add(box: Box): Box {
    this.boxes.push(box)
    this.range(box, k => { const list = this.cells.get(k) ?? []; list.push(box); this.cells.set(k, list) })
    return box
  }

  remove(box: Box): void {
    const i = this.boxes.indexOf(box); if (i >= 0) this.boxes.splice(i, 1)
    this.range(box, k => { const list = this.cells.get(k); if (list) { const j = list.indexOf(box); if (j >= 0) list.splice(j, 1) } })
  }

  near(x0: number, z0: number, x1: number, z1: number): Box[] {
    const out = new Set<Box>()
    this.range({ x0: Math.min(x0, x1), z0: Math.min(z0, z1), x1: Math.max(x0, x1), z1: Math.max(z0, z1) }, k => this.cells.get(k)?.forEach(b => out.add(b)))
    return [...out]
  }

  /** Pushes a circle out of every box it overlaps and keeps it inside the map. */
  resolve(p: { x: number; z: number }, r: number): void {
    for (const b of this.near(p.x - r, p.z - r, p.x + r, p.z + r)) {
      const cx = Math.max(b.x0, Math.min(p.x, b.x1)), cz = Math.max(b.z0, Math.min(p.z, b.z1))
      const dx = p.x - cx, dz = p.z - cz, d2 = dx * dx + dz * dz
      if (d2 >= r * r) continue
      if (d2 > 1e-6) { const d = Math.sqrt(d2), push = r - d; p.x += (dx / d) * push; p.z += (dz / d) * push }
      else {
        // Centre is inside the box: leave by the nearest face.
        const opts = [[p.x - b.x0, -1, 0], [b.x1 - p.x, 1, 0], [p.z - b.z0, 0, -1], [b.z1 - p.z, 0, 1]] as const
        const [dist, ox, oz] = opts.reduce((a, o) => (o[0] < a[0] ? o : a))
        p.x += ox * (dist + r); p.z += oz * (dist + r)
      }
    }
    const B = this.bounds
    p.x = Math.max(B.x0 + r, Math.min(B.x1 - r, p.x)); p.z = Math.max(B.z0 + r, Math.min(B.z1 - r, p.z))
  }

  /** First shot-blocking box hit on the segment a→b, as a 0..1 fraction (1 = clear). */
  segment(ax: number, az: number, bx: number, bz: number): number {
    let best = 1
    const dx = bx - ax, dz = bz - az
    for (const b of this.near(ax, az, bx, bz)) {
      if (!b.blocksShots) continue
      let t0 = 0, t1 = 1, ok = true
      for (const [p, d, lo, hi] of [[ax, dx, b.x0, b.x1], [az, dz, b.z0, b.z1]] as const) {
        if (Math.abs(d) < 1e-9) { if (p < lo || p > hi) { ok = false; break } continue }
        let ta = (lo - p) / d, tb = (hi - p) / d
        if (ta > tb) [ta, tb] = [tb, ta]
        t0 = Math.max(t0, ta); t1 = Math.min(t1, tb)
        if (t0 > t1) { ok = false; break }
      }
      if (ok && t0 < best) best = t0
    }
    return best
  }

  clear(ax: number, az: number, bx: number, bz: number): boolean { return this.segment(ax, az, bx, bz) >= 1 }
}

/** Small deterministic RNG so maps are the same every match. */
export function rng(seed: number): () => number {
  let s = seed >>> 0
  return () => { s = (s + 0x6d2b79f5) >>> 0; let t = s; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296 }
}

export const PALETTE = { sand: 0xc9b48a, stone: 0xa89a80, clay: 0xb98b62, concrete: 0x8f948c, dark: 0x6e6658, wall: 0x8a7a62, steel: 0x5f6b6e, rock: 0x8b7b62, rockDark: 0x6f6250 }

/** Collects solid boxes, registers them for collision and draws them as one instanced mesh. */
export class Solids {
  readonly list: { x: number; z: number; w: number; d: number; h: number; color: number }[] = []
  constructor(private readonly collision: Collision, private readonly jitter = rng(77)) {}

  add(cx: number, cz: number, w: number, d: number, h: number, color: number, blocksShots = true, tag?: string): Box {
    this.list.push({ x: cx, z: cz, w, d, h, color })
    return this.collision.add({ x0: cx - w / 2, z0: cz - d / 2, x1: cx + w / 2, z1: cz + d / 2, h, blocksShots, ...(tag ? { tag } : {}) })
  }

  overlaps(cx: number, cz: number, r: number, gap = 5): boolean {
    return this.list.some(s => Math.abs(cx - s.x) < s.w / 2 + r + gap && Math.abs(cz - s.z) < s.d / 2 + r + gap)
  }

  build(scene: THREE.Scene): THREE.InstancedMesh {
    const inst = new THREE.InstancedMesh(new THREE.BoxGeometry(1, 1, 1), new THREE.MeshLambertMaterial({ color: 0xffffff }), Math.max(1, this.list.length))
    const m = new THREE.Matrix4(), c = new THREE.Color(), q = new THREE.Quaternion()
    this.list.forEach((s, i) => {
      m.compose(new THREE.Vector3(s.x, s.h / 2, s.z), q, new THREE.Vector3(s.w, s.h, s.d))
      inst.setMatrixAt(i, m)
      c.setHex(s.color).offsetHSL(0, 0, (this.jitter() - 0.5) * 0.06)
      inst.setColorAt(i, c)
    })
    inst.count = this.list.length
    scene.add(inst)
    return inst
  }
}

export function groundPatch(scene: THREE.Scene, x: number, z: number, w: number, d: number, color: number, y = 0.02): THREE.Mesh {
  const m = new THREE.Mesh(new THREE.PlaneGeometry(w, d), new THREE.MeshLambertMaterial({ color }))
  m.rotation.x = -Math.PI / 2; m.position.set(x, y, z); scene.add(m); return m
}

/** A flat ribbon on the ground along a polyline (roads, riverbeds). */
export function groundRibbon(scene: THREE.Scene, pts: readonly { x: number; z: number }[], width: number, color: number, y = 0.03): THREE.Mesh {
  const pos: number[] = [], idx: number[] = []
  pts.forEach((p, i) => {
    const a = pts[Math.max(0, i - 1)]!, b = pts[Math.min(pts.length - 1, i + 1)]!
    const tx = b.x - a.x, tz = b.z - a.z, l = Math.hypot(tx, tz) || 1
    const nx = -tz / l * width / 2, nz = tx / l * width / 2
    pos.push(p.x + nx, y, p.z + nz, p.x - nx, y, p.z - nz)
    if (i > 0) { const k = i * 2; idx.push(k - 2, k - 1, k, k - 1, k + 1, k) }
  })
  const g = new THREE.BufferGeometry()
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); g.setIndex(idx); g.computeVertexNormals()
  const m = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ color, side: THREE.DoubleSide }))
  scene.add(m); return m
}

/** Polyline helpers for convoy routes. */
export class Path {
  readonly cum: number[] = [0]
  constructor(readonly pts: readonly { x: number; z: number }[]) {
    for (let i = 1; i < pts.length; i++) this.cum.push(this.cum[i - 1]! + Math.hypot(pts[i]!.x - pts[i - 1]!.x, pts[i]!.z - pts[i - 1]!.z))
  }
  get length(): number { return this.cum.at(-1)! }
  at(s: number): { x: number; z: number; angle: number } {
    s = Math.max(0, Math.min(this.length, s))
    let i = 1
    while (i < this.pts.length - 1 && this.cum[i]! < s) i++
    const a = this.pts[i - 1]!, b = this.pts[i]!, seg = this.cum[i]! - this.cum[i - 1]! || 1, t = (s - this.cum[i - 1]!) / seg
    return { x: a.x + (b.x - a.x) * t, z: a.z + (b.z - a.z) * t, angle: Math.atan2(b.x - a.x, b.z - a.z) }
  }
  /** Distance along the path of the vertex nearest to a point. */
  sOf(p: { x: number; z: number }): number {
    let best = 0, bd = Infinity
    this.pts.forEach((q, i) => { const d = Math.hypot(q.x - p.x, q.z - p.z); if (d < bd) { bd = d; best = this.cum[i]! } })
    return best
  }
  /** Shortest distance from a point to the polyline. */
  distance(p: { x: number; z: number }): number {
    let best = Infinity
    for (let i = 1; i < this.pts.length; i++) {
      const a = this.pts[i - 1]!, b = this.pts[i]!, dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz || 1
      const t = Math.max(0, Math.min(1, ((p.x - a.x) * dx + (p.z - a.z) * dz) / l2))
      best = Math.min(best, Math.hypot(p.x - (a.x + dx * t), p.z - (a.z + dz * t)))
    }
    return best
  }
}
