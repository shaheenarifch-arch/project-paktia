import * as THREE from 'three'

// Gardun, laid out from docs/gardun-map.md (north is -z, South Gate at +z).
// 1 world unit = 4 m, so the 2.5 km core is 625 units across. toData() maps
// world units back to the game-space metres the domain layer uses (for
// example the Medical Station protected zone at 1180, 1320).
export const HALF = 312
export const METRES_PER_UNIT = 4
export const toData = (x: number, z: number) => ({ x: 1250 + x * METRES_PER_UNIT, y: 0, z: 1250 + z * METRES_PER_UNIT })

export const SOUTH_GATE = { x: 0, z: 272 }
export const CAPTURE_RADIUS = 12

export interface ObjectiveSite { x: number; z: number; label: string; sector: string }
export const OBJECTIVES: Record<string, ObjectiveSite> = {
  capture_airfield: { x: -200, z: 190, label: 'Airfield', sector: 'airfield' },
  secure_industrial_depot: { x: 190, z: 185, label: 'Industrial Depot', sector: 'industrial' },
  west_ridge_radio: { x: -235, z: 25, label: 'West Ridge Radio', sector: 'west_ridge' },
  government_relay: { x: 150, z: 40, label: 'Government Relay', sector: 'government' },
  market_square: { x: 55, z: -30, label: 'Market Square', sector: 'old_gardun' },
  medical_station: { x: -17.5, z: 17.5, label: 'Medical Station', sector: 'old_gardun' },
  municipal_ruins: { x: -70, z: -55, label: 'Municipal Ruins', sector: 'old_gardun' },
  mountain_fort: { x: -215, z: -185, label: 'Mountain Fort', sector: 'mountain_fort' },
  east_power_station: { x: 215, z: -150, label: 'East Power Station', sector: 'east_power' },
  outer_gate: { x: 0, z: -172, label: 'Outer Gate', sector: 'citadel' },
  inner_courtyard: { x: 0, z: -222, label: 'Inner Courtyard', sector: 'citadel' },
  command_hall: { x: -32, z: -258, label: 'Command Hall', sector: 'citadel' },
  operations_room: { x: 32, z: -272, label: 'Operations Room', sector: 'citadel' }
}

// The Dry Riverbed crosses the city; it can only be crossed at these points.
export const RIVER = { z0: 100, z1: 118 }
export const CROSSINGS = [
  { id: 'riverbed_bridge', x0: -12, x1: 12 },
  { id: 'west_ford', x0: -152, x1: -126 },
  { id: 'east_crossing', x0: 112, x1: 132 }
]
// Opened by the tunnel_discovery surprise event (old_gardun_cellars).
export const CELLARS = { a: { x: -42, z: 82 }, b: { x: -78, z: -30 } }

export interface Box { x0: number; z0: number; x1: number; z1: number; h: number; blocksShots: boolean; tag?: string }

/** Uniform grid so movement and bullets only test nearby boxes. */
export class Collision {
  private readonly cell = 24
  private readonly cells = new Map<number, Box[]>()
  readonly boxes: Box[] = []

  private key(cx: number, cz: number): number { return (cx + 100) * 1000 + (cz + 100) }
  private range(b: { x0: number; z0: number; x1: number; z1: number }, fn: (k: number) => void): void {
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

  /** Pushes a circle out of every box it overlaps. */
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
    p.x = Math.max(-HALF + r, Math.min(HALF - r, p.x)); p.z = Math.max(-HALF + r, Math.min(HALF - r, p.z))
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

// Small deterministic RNG so Gardun's buildings are the same every match.
function rng(seed: number): () => number {
  let s = seed >>> 0
  return () => { s = (s + 0x6d2b79f5) >>> 0; let t = s; t = Math.imul(t ^ (t >>> 15), t | 1); t ^= t + Math.imul(t ^ (t >>> 7), t | 61); return ((t ^ (t >>> 14)) >>> 0) / 4294967296 }
}

const PALETTE = { sand: 0xc9b48a, stone: 0xa89a80, clay: 0xb98b62, concrete: 0x8f948c, dark: 0x6e6658, wall: 0x8a7a62, steel: 0x5f6b6e }

export interface BuiltWorld {
  collision: Collision
  bridgeBarrier: { box: Box; mesh: THREE.Mesh }
  cellarMeshes: THREE.Object3D[]
  objectiveMeshes: Map<string, { group: THREE.Group; ring: THREE.Mesh; fill: THREE.Mesh; beacon: THREE.Mesh }>
}

export function buildWorld(scene: THREE.Scene): BuiltWorld {
  const collision = new Collision()
  const rand = rng(4219)

  // Ground and sector tints.
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(HALF * 2 + 200, HALF * 2 + 200), new THREE.MeshLambertMaterial({ color: 0xb7a27a }))
  ground.rotation.x = -Math.PI / 2
  scene.add(ground)
  const patch = (x: number, z: number, w: number, d: number, color: number, y = 0.02) => {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, d), new THREE.MeshLambertMaterial({ color }))
    m.rotation.x = -Math.PI / 2; m.position.set(x, y, z); scene.add(m); return m
  }
  patch(-200, 190, 150, 90, 0xa59674)                        // airfield apron
  patch(-200, 190, 130, 14, 0x55524b, 0.04)                  // runway
  patch(190, 185, 140, 100, 0x9d978a)                        // industrial ward
  patch(0, -20, 240, 200, 0xc2a97f)                          // Old Gardun
  patch(-215, -185, 120, 120, 0x9a8c6c)                      // Mountain Fort plateau
  patch(0, -230, 150, 150, 0xa1937a)                         // Citadel
  patch(0, 30, 16, 520, 0x8d8068, 0.03)                      // main avenue
  patch(0, RIVER.z0 + (RIVER.z1 - RIVER.z0) / 2, HALF * 2, RIVER.z1 - RIVER.z0, 0x8f7a55, 0.05) // dry riverbed
  patch(0, 272, 90, 50, 0x7d8a6a, 0.04)                      // South Gate staging

  // Riverbed banks: impassable on foot except at crossings; shots pass over.
  let x = -HALF
  for (const c of [...CROSSINGS].sort((a, b) => a.x0 - b.x0)) {
    collision.add({ x0: x, z0: RIVER.z0, x1: c.x0, z1: RIVER.z1, h: 0, blocksShots: false, tag: 'river' })
    x = c.x1
  }
  collision.add({ x0: x, z0: RIVER.z0, x1: HALF, z1: RIVER.z1, h: 0, blocksShots: false, tag: 'river' })
  for (const c of CROSSINGS) {
    const deck = new THREE.Mesh(new THREE.BoxGeometry(c.x1 - c.x0, 0.4, RIVER.z1 - RIVER.z0 + 4), new THREE.MeshLambertMaterial({ color: c.id === 'riverbed_bridge' ? 0x7c7468 : 0x9b8a64 }))
    deck.position.set((c.x0 + c.x1) / 2, 0.2, (RIVER.z0 + RIVER.z1) / 2); scene.add(deck)
  }

  // Every solid box (buildings and walls) goes into one instanced mesh.
  const solids: { x: number; z: number; w: number; d: number; h: number; color: number }[] = []
  const solid = (cx: number, cz: number, w: number, d: number, h: number, color: number, tag?: string) => {
    collision.add({ x0: cx - w / 2, z0: cz - d / 2, x1: cx + w / 2, z1: cz + d / 2, h, blocksShots: true, ...(tag ? { tag } : {}) })
    solids.push({ x: cx, z: cz, w, d, h, color })
  }

  // Map boundary.
  solid(0, -HALF - 3, HALF * 2 + 12, 6, 8, PALETTE.dark); solid(0, HALF + 3, HALF * 2 + 12, 6, 8, PALETTE.dark)
  solid(-HALF - 3, 0, 6, HALF * 2, 8, PALETTE.dark); solid(HALF + 3, 0, 6, HALF * 2, 8, PALETTE.dark)

  // Citadel: outer wall with the gate on the south face, inner wall with a gap.
  const wallH = 11
  solid(-42, -160, 64, 5, wallH, PALETTE.wall); solid(42, -160, 64, 5, wallH, PALETTE.wall)   // south face, gate gap |x|<10
  solid(-72, -230, 5, 145, wallH, PALETTE.wall); solid(72, -230, 5, 145, wallH, PALETTE.wall) // sides
  solid(0, -301, 149, 5, wallH, PALETTE.wall)                                                    // north
  solid(-40, -198, 60, 4, 7, PALETTE.stone); solid(40, -198, 60, 4, 7, PALETTE.stone)           // inner wall, gap |x|<10
  solid(-32, -242, 26, 4, 6, PALETTE.stone); solid(32, -252, 26, 4, 6, PALETTE.stone)           // command / ops halls
  ;[[-62, -164], [62, -164], [-62, -296], [62, -296]].forEach(([tx, tz]) => solid(tx!, tz!, 10, 10, 15, PALETTE.wall))

  // Mountain Fort: walled compound, openings south and east.
  const fx = -215, fz = -185
  solid(fx, fz - 34, 72, 4, 7, PALETTE.wall)
  solid(fx - 34, fz, 4, 72, 7, PALETTE.wall)
  solid(fx - 20, fz + 34, 32, 4, 7, PALETTE.wall); solid(fx + 26, fz + 34, 20, 4, 7, PALETTE.wall)
  solid(fx + 34, fz - 18, 4, 36, 7, PALETTE.wall); solid(fx + 34, fz + 26, 4, 16, 7, PALETTE.wall)

  // East Power Station: transformer blocks and a turbine hall.
  solid(240, -180, 30, 18, 12, PALETTE.steel); solid(190, -125, 8, 8, 6, PALETTE.steel); solid(240, -125, 8, 8, 6, PALETTE.steel)
  solid(196, -170, 6, 6, 18, PALETTE.concrete)

  // Airfield hangars and control tower; industrial warehouses.
  solid(-245, 155, 34, 22, 10, PALETTE.steel); solid(-155, 155, 34, 22, 10, PALETTE.steel); solid(-185, 225, 8, 8, 16, PALETTE.concrete)
  solid(150, 150, 36, 24, 9, PALETTE.concrete); solid(235, 150, 30, 26, 9, PALETTE.concrete); solid(230, 220, 40, 18, 8, PALETTE.steel)
  ;[[175, 205], [205, 205], [165, 168]].forEach(([cx, cz]) => solid(cx!, cz!, 6, 12, 3, 0x7a6b4d)) // container stacks

  // Government Quarter and West Ridge relay.
  solid(115, 20, 22, 30, 14, PALETTE.concrete); solid(185, 20, 22, 30, 14, PALETTE.concrete); solid(150, 0, 30, 12, 10, PALETTE.stone)
  solid(-260, 10, 14, 14, 6, PALETTE.concrete); solid(-210, 45, 10, 18, 5, PALETTE.stone)

  // South Gate staging walls (open to the north).
  solid(-50, 272, 6, 40, 5, PALETTE.wall); solid(50, 272, 6, 40, 5, PALETTE.wall)

  // Scatter the city blocks. Old Gardun is dense; the rest is sparser.
  const keepClear = (cx: number, cz: number, r: number) => {
    if (Math.abs(cx) < 12 + r && cz > -165) return false                                    // main avenue
    if (cz + r > RIVER.z0 - 6 && cz - r < RIVER.z1 + 6) return false                         // riverbed
    if (Math.hypot(cx - SOUTH_GATE.x, cz - SOUTH_GATE.z) < 60 + r) return false
    if (cz < -150 && Math.abs(cx) < 80 + r) return false                                    // citadel
    if (Math.hypot(cx + 215, cz + 185) < 45 + r) return false                                // fort
    if (Math.hypot(cx - CELLARS.a.x, cz - CELLARS.a.z) < 12 + r || Math.hypot(cx - CELLARS.b.x, cz - CELLARS.b.z) < 12 + r) return false
    if (Math.abs(cz - 190) < 12 + r && cx < -130 && cx > -270) return false                  // runway
    for (const o of Object.values(OBJECTIVES)) if (Math.hypot(cx - o.x, cz - o.z) < 24 + r) return false
    for (const s of solids) if (Math.abs(cx - s.x) < s.w / 2 + r + 5 && Math.abs(cz - s.z) < s.d / 2 + r + 5) return false
    return true
  }
  const scatter = (count: number, x0: number, z0: number, x1: number, z1: number, min: number, max: number, hMin: number, hMax: number, colors: number[]) => {
    for (let i = 0, tries = 0; i < count && tries < count * 40; tries++) {
      const w = min + rand() * (max - min), d = min + rand() * (max - min)
      const cx = x0 + rand() * (x1 - x0), cz = z0 + rand() * (z1 - z0)
      if (!keepClear(cx, cz, Math.max(w, d) / 2)) continue
      solid(cx, cz, w, d, hMin + rand() * (hMax - hMin), colors[Math.floor(rand() * colors.length)]!)
      i++
    }
  }
  scatter(70, -120, -120, 120, 80, 7, 15, 4, 11, [PALETTE.sand, PALETTE.clay, PALETTE.stone])     // Old Gardun
  scatter(26, 80, -110, 290, 90, 9, 18, 5, 12, [PALETTE.concrete, PALETTE.stone, PALETTE.sand])   // east districts
  scatter(26, -295, -120, -110, 90, 8, 16, 4, 10, [PALETTE.clay, PALETTE.sand, PALETTE.stone])    // west ridge
  scatter(22, -295, 125, 295, 250, 8, 16, 4, 9, [PALETTE.sand, PALETTE.concrete, PALETTE.clay])   // southern ward
  scatter(14, -295, -300, -100, -130, 8, 14, 4, 8, [PALETTE.stone, PALETTE.clay])                 // north-west
  scatter(14, 100, -300, 295, -130, 8, 14, 4, 8, [PALETTE.stone, PALETTE.concrete])               // north-east

  const inst = new THREE.InstancedMesh(new THREE.BoxGeometry(1, 1, 1), new THREE.MeshLambertMaterial({ color: 0xffffff }), solids.length)
  const m = new THREE.Matrix4(), c = new THREE.Color()
  solids.forEach((s, i) => {
    m.compose(new THREE.Vector3(s.x, s.h / 2, s.z), new THREE.Quaternion(), new THREE.Vector3(s.w, s.h, s.d))
    inst.setMatrixAt(i, m)
    c.setHex(s.color).offsetHSL(0, 0, (rand() - 0.5) * 0.06)
    inst.setColorAt(i, c)
  })
  scene.add(inst)

  // Bridge barricade (bridge_obstruction surprise event). Hidden until then.
  const bc = CROSSINGS[0]!
  const barrier = { x0: bc.x0, z0: RIVER.z0 - 2, x1: bc.x1, z1: RIVER.z1 + 2, h: 3, blocksShots: true, tag: 'barrier' }
  const barrierMesh = new THREE.Mesh(new THREE.BoxGeometry(bc.x1 - bc.x0, 3, 6), new THREE.MeshLambertMaterial({ color: 0x4a3f33 }))
  barrierMesh.position.set(0, 1.5, (RIVER.z0 + RIVER.z1) / 2); barrierMesh.visible = false; scene.add(barrierMesh)

  // Cellar hatches (tunnel_discovery surprise event).
  const cellarMeshes = [CELLARS.a, CELLARS.b].map(p => {
    const g = new THREE.Group()
    const hatch = new THREE.Mesh(new THREE.CylinderGeometry(3, 3, 0.4, 20), new THREE.MeshLambertMaterial({ color: 0x2b2620 }))
    const glow = new THREE.Mesh(new THREE.RingGeometry(3.6, 4.4, 32), new THREE.MeshBasicMaterial({ color: 0xffd166, transparent: true, opacity: 0.8, side: THREE.DoubleSide }))
    glow.rotation.x = -Math.PI / 2; glow.position.y = 0.25
    g.add(hatch, glow); g.position.set(p.x, 0.2, p.z); g.visible = false; scene.add(g)
    return g
  })

  // Objective markers: a ring on the ground, a capture-progress disc and a beacon.
  const objectiveMeshes = new Map<string, { group: THREE.Group; ring: THREE.Mesh; fill: THREE.Mesh; beacon: THREE.Mesh }>()
  for (const [id, o] of Object.entries(OBJECTIVES)) {
    const group = new THREE.Group()
    const ring = new THREE.Mesh(new THREE.RingGeometry(CAPTURE_RADIUS - 0.8, CAPTURE_RADIUS, 48), new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.9, side: THREE.DoubleSide }))
    ring.rotation.x = -Math.PI / 2; ring.position.y = 0.12
    const fill = new THREE.Mesh(new THREE.CircleGeometry(CAPTURE_RADIUS - 0.8, 48), new THREE.MeshBasicMaterial({ color: 0x78ffab, transparent: true, opacity: 0.28, side: THREE.DoubleSide }))
    fill.rotation.x = -Math.PI / 2; fill.position.y = 0.1; fill.scale.setScalar(0.001)
    const beacon = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, 40, 8, 1, true), new THREE.MeshBasicMaterial({ color: 0xffd166, transparent: true, opacity: 0.35, depthWrite: false }))
    beacon.position.y = 20
    const flag = new THREE.Mesh(new THREE.CylinderGeometry(0.25, 0.25, 7, 6), new THREE.MeshLambertMaterial({ color: 0x3c3a36 }))
    flag.position.y = 3.5
    group.add(ring, fill, beacon, flag)
    group.position.set(o.x, 0, o.z)
    group.userData.id = id
    scene.add(group)
    objectiveMeshes.set(id, { group, ring, fill, beacon })
  }

  return { collision, bridgeBarrier: { box: barrier, mesh: barrierMesh }, cellarMeshes, objectiveMeshes }
}
