import * as THREE from 'three'
import { E02_POINTS } from '../../src/index.js'
import { Collision, PALETTE, Path, Solids, groundPatch, groundRibbon, rng, type Box } from './collision'
import { hatchMesh } from './gardun'

// The Silent Corridor: the valley from Checkpoint Echo (origin) down to
// Gardun's South Gate. Episode 02 positions are game-space metres from
// Checkpoint Echo; here 1 unit = 4 m and "forward" is -z on screen, so
// world (x, z) = (data.x / 4, -data.z / 4) and back again with toData().
export const M = 4
export const toWorld = (p: { x: number; z: number }) => ({ x: p.x / M, z: -p.z / M })
export const toData = (x: number, z: number) => ({ x: x * M, y: 0, z: -z * M })

export const ECHO = { x: 0, z: 0 }
export const RELAY_KILO = toWorld(E02_POINTS.relayKilo)          // (150, -225)
export const CORRIDOR_END = toWorld(E02_POINTS.corridorEnd)      // (425, -375)
export const NARROWS = toWorld(E02_POINTS.ambushNarrows)         // (312.5, -412.5)
export const SOUTH_GATE = { x: 505, z: -545 }
export const BRIDGE = { x: 380, z: -350 }
export const BOUNDS = { x0: -80, z0: -620, x1: 600, z1: 70 }

// Main road: Echo → Relay Kilo shoulder → bridge → corridor end → South Gate.
const APPROACH = { x: 330, z: -320 }
const ROAD_PTS = [ECHO, { x: 30, z: -80 }, { x: 95, z: -170 }, { x: 165, z: -232 }, { x: 250, z: -280 }, APPROACH, BRIDGE, CORRIDOR_END, { x: 465, z: -450 }, SOUTH_GATE]
// The Dry Riverbed runs under the bridge and through the narrows.
const RIVER_PTS = [{ x: 560, z: -250 }, { x: 460, z: -305 }, BRIDGE, { x: 335, z: -385 }, NARROWS, { x: 322, z: -462 }, { x: 375, z: -505 }, { x: 455, z: -528 }, { x: 590, z: -545 }]
export const RIVER_WIDTH = 26

// Convoy routes share the road up to the bridge approach so vehicles keep their place when the route changes.
export const ROUTES = {
  main: new Path(ROAD_PTS),
  riverbed: new Path([...ROAD_PTS.slice(0, ROAD_PTS.indexOf(APPROACH) + 1), { x: 352, z: -360 }, { x: 335, z: -385 }, NARROWS, { x: 322, z: -462 }, { x: 375, z: -505 }, { x: 440, z: -524 }, { x: 478, z: -540 }, SOUTH_GATE])
}
export const S = {
  outage: ROUTES.main.sOf({ x: 250, z: -280 }),
  approach: ROUTES.main.sOf(APPROACH) - 6,
  bridgeWreck: ROUTES.main.sOf(BRIDGE) - 14,
  narrowsHold: ROUTES.riverbed.sOf({ x: 335, z: -385 }),
  end: { main: ROUTES.main.length - 4, riverbed: ROUTES.riverbed.length - 4 }
}

// Drainage tunnel: near bank by the bridge to the far bank (infantry only).
export const DRAIN = { a: { x: 352, z: -330 }, b: { x: 398, z: -414 } }

export interface BuiltCorridor {
  collision: Collision
  wreck: { box: Box; mesh: THREE.Group }
  rampGate: { box: Box; mesh: THREE.Mesh }
  drainHatches: THREE.Group[]
  relayMesh: THREE.Group
  relayZone: THREE.Mesh
  endMarker: THREE.Group
  yardMarker: THREE.Mesh
}

export function buildCorridor(scene: THREE.Scene): BuiltCorridor {
  const collision = new Collision(BOUNDS)
  const solids = new Solids(collision)
  const rand = rng(90210)
  const road = ROUTES.main, river = new Path(RIVER_PTS)

  groundPatch(scene, (BOUNDS.x0 + BOUNDS.x1) / 2, (BOUNDS.z0 + BOUNDS.z1) / 2, 900, 900, 0xa8946c, 0)
  groundRibbon(scene, RIVER_PTS, RIVER_WIDTH, 0x8a7350, 0.04)
  groundRibbon(scene, ROAD_PTS, 12, 0x6f6658, 0.06)
  groundPatch(scene, ECHO.x, ECHO.z + 10, 70, 60, 0x7d8a6a, 0.05)           // Checkpoint Echo
  groundPatch(scene, SOUTH_GATE.x, SOUTH_GATE.z, 80, 60, 0x7d8a6a, 0.05)    // South Gate yard

  // Valley walls: rock everywhere outside the corridor.
  const inCorridor = (x: number, z: number) =>
    road.distance({ x, z }) < 62 || river.distance({ x, z }) < 34 ||
    Math.hypot(x - ECHO.x, z - ECHO.z) < 60 || Math.hypot(x - SOUTH_GATE.x, z - SOUTH_GATE.z) < 70 || Math.hypot(x - RELAY_KILO.x, z - RELAY_KILO.z) < 30
  const cell = 16
  for (let x = BOUNDS.x0; x < BOUNDS.x1; x += cell) for (let z = BOUNDS.z0; z < BOUNDS.z1; z += cell) {
    const cx = x + cell / 2, cz = z + cell / 2
    if (inCorridor(cx, cz)) continue
    const near = road.distance({ x: cx, z: cz }) < 90 || river.distance({ x: cx, z: cz }) < 60
    solids.add(cx, cz, cell + 1, cell + 1, near ? 7 + rand() * 9 : 14 + rand() * 12, rand() < 0.5 ? PALETTE.rock : PALETTE.rockDark)
  }
  // Boulders inside the corridor for cover, away from the road and riverbed.
  for (let i = 0, tries = 0; i < 60 && tries < 2000; tries++) {
    const x = BOUNDS.x0 + rand() * (BOUNDS.x1 - BOUNDS.x0), z = BOUNDS.z0 + rand() * (BOUNDS.z1 - BOUNDS.z0)
    if (!inCorridor(x, z) || road.distance({ x, z }) < 14 || river.distance({ x, z }) < RIVER_WIDTH / 2 + 6) continue
    if (Math.hypot(x - ECHO.x, z - ECHO.z) < 40 || Math.hypot(x - SOUTH_GATE.x, z - SOUTH_GATE.z) < 45 || Math.hypot(x - RELAY_KILO.x, z - RELAY_KILO.z) < 14) continue
    if (Math.hypot(x - DRAIN.a.x, z - DRAIN.a.z) < 10 || Math.hypot(x - DRAIN.b.x, z - DRAIN.b.z) < 10) continue
    const s = 3 + rand() * 5
    if (solids.overlaps(x, z, s / 2, 3)) continue
    solids.add(x, z, s, s * (0.7 + rand() * 0.6), 2 + rand() * 3, PALETTE.rock)
    i++
  }
  // Checkpoint Echo and South Gate walls.
  solids.add(ECHO.x - 30, ECHO.z + 12, 4, 40, 4, PALETTE.wall); solids.add(ECHO.x + 30, ECHO.z + 12, 4, 40, 4, PALETTE.wall)
  solids.add(SOUTH_GATE.x - 38, SOUTH_GATE.z, 4, 46, 6, PALETTE.wall); solids.add(SOUTH_GATE.x + 38, SOUTH_GATE.z, 4, 46, 6, PALETTE.wall)
  solids.add(SOUTH_GATE.x, SOUTH_GATE.z - 26, 80, 4, 8, PALETTE.wall)
  ;[[-14, -8], [12, 6], [-6, 12]].forEach(([dx, dz]) => solids.add(SOUTH_GATE.x + dx!, SOUTH_GATE.z + dz!, 5, 9, 3, 0x7a6b4d))
  solids.build(scene)

  // Riverbed banks: low walls on both edges (shots pass over). Gaps where the
  // road crosses (the bridge), at the near-side ramp and the far-side exit.
  const gaps = [{ ...BRIDGE, r: 9 }, { x: 352, z: -360, r: 9 }, { x: 466, z: -530, r: 12 }]
  for (let i = 1; i < RIVER_PTS.length; i++) {
    const a = RIVER_PTS[i - 1]!, b = RIVER_PTS[i]!, len = Math.hypot(b.x - a.x, b.z - a.z)
    const nx = -(b.z - a.z) / len, nz = (b.x - a.x) / len
    for (let t = 0; t < len; t += 3) {
      for (const side of [-1, 1]) {
        const x = a.x + (b.x - a.x) * (t / len) + nx * side * (RIVER_WIDTH / 2), z = a.z + (b.z - a.z) * (t / len) + nz * side * (RIVER_WIDTH / 2)
        if (gaps.some(g => Math.hypot(x - g.x, z - g.z) < g.r)) continue
        collision.add({ x0: x - 2, z0: z - 2, x1: x + 2, z1: z + 2, h: 0, blocksShots: false, tag: 'bank' })
      }
    }
  }
  // Bridge deck over the riverbed.
  const deck = new THREE.Mesh(new THREE.BoxGeometry(14, 0.6, RIVER_WIDTH + 8), new THREE.MeshLambertMaterial({ color: 0x7c7468 }))
  deck.position.set(BRIDGE.x, 0.5, BRIDGE.z)
  deck.lookAt(CORRIDOR_END.x, 0.5, CORRIDOR_END.z); scene.add(deck)

  // Bank edge strips so the riverbed reads as low ground.
  for (const side of [-1, 1]) {
    const edge = RIVER_PTS.map((p, i) => {
      const a = RIVER_PTS[Math.max(0, i - 1)]!, b = RIVER_PTS[Math.min(RIVER_PTS.length - 1, i + 1)]!
      const l = Math.hypot(b.x - a.x, b.z - a.z) || 1
      return { x: p.x - (b.z - a.z) / l * side * RIVER_WIDTH / 2, z: p.z + (b.x - a.x) / l * side * RIVER_WIDTH / 2 }
    })
    groundRibbon(scene, edge, 2.4, 0x5f5040, 0.07)
  }

  // Wreck blocking the bridge until Warden-1 pushes it off.
  const wreckBox: Box = { x0: BRIDGE.x - 8, z0: BRIDGE.z - 7, x1: BRIDGE.x + 8, z1: BRIDGE.z + 7, h: 3, blocksShots: true, tag: 'wreck' }
  collision.add(wreckBox)
  const wreck = new THREE.Group()
  const hull = new THREE.Mesh(new THREE.BoxGeometry(5, 2.6, 10), new THREE.MeshLambertMaterial({ color: 0x3b3833 }))
  hull.position.y = 1.3; hull.rotation.y = 0.7
  const cab = new THREE.Mesh(new THREE.BoxGeometry(4, 2, 3.5), new THREE.MeshLambertMaterial({ color: 0x2a2724 }))
  cab.position.set(2.5, 2.6, -2); cab.rotation.y = 0.7
  wreck.add(hull, cab); wreck.position.set(BRIDGE.x, 0, BRIDGE.z); scene.add(wreck)

  // Rubble closing the near-side ramp into the riverbed until the detour is chosen.
  const rampGate: Box = { x0: 344, z0: -368, x1: 360, z1: -352, h: 2, blocksShots: false, tag: 'ramp' }
  collision.add(rampGate)
  const rubble = new THREE.Mesh(new THREE.BoxGeometry(16, 1.6, 16), new THREE.MeshLambertMaterial({ color: 0x6b5e4a }))
  rubble.position.set(352, 0.8, -360); scene.add(rubble)

  const drainHatches = [DRAIN.a, DRAIN.b].map(p => { const g = hatchMesh(); g.position.set(p.x, 0.2, p.z); g.visible = false; scene.add(g); return g })

  // Relay Kilo: a mast once placed, and the shoulder zone where it should go.
  const relayMesh = new THREE.Group()
  const mast = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.5, 9, 6), new THREE.MeshLambertMaterial({ color: 0x3c4a44 }))
  mast.position.y = 4.5
  const dish = new THREE.Mesh(new THREE.SphereGeometry(1.1, 10, 8), new THREE.MeshLambertMaterial({ color: 0x8fd3ff, emissive: 0x1b3a4a }))
  dish.position.y = 9.2; dish.name = 'light'
  relayMesh.add(mast, dish); relayMesh.visible = false; scene.add(relayMesh)
  const relayZone = new THREE.Mesh(new THREE.RingGeometry(24, 25.5, 48), new THREE.MeshBasicMaterial({ color: 0x8fd3ff, transparent: true, opacity: 0.7, side: THREE.DoubleSide }))
  relayZone.rotation.x = -Math.PI / 2; relayZone.position.set(RELAY_KILO.x, 0.15, RELAY_KILO.z); scene.add(relayZone)

  const endMarker = new THREE.Group()
  const ring = new THREE.Mesh(new THREE.RingGeometry(14, 15.5, 48), new THREE.MeshBasicMaterial({ color: 0xffd166, transparent: true, opacity: 0.8, side: THREE.DoubleSide }))
  ring.rotation.x = -Math.PI / 2; ring.position.y = 0.15
  const beacon = new THREE.Mesh(new THREE.CylinderGeometry(0.5, 0.5, 50, 8, 1, true), new THREE.MeshBasicMaterial({ color: 0xffd166, transparent: true, opacity: 0.35, depthWrite: false }))
  beacon.position.y = 25
  endMarker.add(ring, beacon); endMarker.position.set(CORRIDOR_END.x, 0, CORRIDOR_END.z); scene.add(endMarker)

  const yardMarker = new THREE.Mesh(new THREE.RingGeometry(30, 31.5, 64), new THREE.MeshBasicMaterial({ color: 0xffd166, transparent: true, opacity: 0.7, side: THREE.DoubleSide }))
  yardMarker.rotation.x = -Math.PI / 2; yardMarker.position.set(SOUTH_GATE.x, 0.15, SOUTH_GATE.z); yardMarker.visible = false; scene.add(yardMarker)

  return { collision, wreck: { box: wreckBox, mesh: wreck }, rampGate: { box: rampGate, mesh: rubble }, drainHatches, relayMesh, relayZone, endMarker, yardMarker }
}

export const MAP_LINES = { road: ROAD_PTS, river: RIVER_PTS }
