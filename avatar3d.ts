import * as THREE from 'three'

export function mountHologramAvatar(canvas: HTMLCanvasElement): () => void {
  const renderer = new THREE.WebGLRenderer({ canvas, alpha: true, antialias: true })
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
  renderer.setClearColor(0x000000, 0)

  const scene = new THREE.Scene()
  const camera = new THREE.PerspectiveCamera(28, 1, 0.1, 100)
  camera.position.set(0, 0.22, 4.3)

  const avatar = new THREE.Group()
  scene.add(avatar)

  const holo = new THREE.MeshStandardMaterial({
    color: 0x152b38,
    roughness: 0.36,
    metalness: 0.55,
    emissive: 0x06394a,
    emissiveIntensity: 0.7
  })
  const accent = new THREE.MeshStandardMaterial({
    color: 0x3fffe1,
    emissive: 0x16d7d3,
    emissiveIntensity: 1.5,
    roughness: 0.25,
    metalness: 0.6
  })

  const head = new THREE.Mesh(new THREE.IcosahedronGeometry(0.48, 4), holo)
  head.scale.set(0.82, 1.08, 0.82)
  head.position.y = 0.45
  avatar.add(head)

  const visor = new THREE.Mesh(new THREE.SphereGeometry(0.43, 32, 16, 0, Math.PI * 2, 0.88, 0.52), accent)
  visor.scale.set(0.86, 0.42, 0.32)
  visor.position.set(0, 0.52, 0.39)
  avatar.add(visor)

  const neck = new THREE.Mesh(new THREE.CylinderGeometry(0.19, 0.25, 0.34, 24), holo)
  neck.position.y = -0.02
  avatar.add(neck)

  const torso = new THREE.Mesh(new THREE.SphereGeometry(0.92, 40, 22), holo)
  torso.scale.set(1.1, 0.56, 0.54)
  torso.position.y = -0.66
  avatar.add(torso)

  const core = new THREE.Mesh(new THREE.TorusGeometry(0.18, 0.018, 12, 48), accent)
  core.rotation.x = Math.PI / 2
  core.position.set(0, -0.51, 0.5)
  avatar.add(core)

  scene.add(new THREE.HemisphereLight(0x9cecff, 0x02050b, 1.4))
  const rim = new THREE.PointLight(0x28ffe4, 8, 7)
  rim.position.set(-1.5, 1.4, 2.2)
  scene.add(rim)
  const fill = new THREE.PointLight(0x637cff, 5, 6)
  fill.position.set(1.8, 0.5, 1.4)
  scene.add(fill)

  let pointerX = 0
  let pointerY = 0
  const onPointer = (e: PointerEvent) => {
    const r = canvas.getBoundingClientRect()
    pointerX = ((e.clientX - r.left) / r.width - 0.5) * 0.7
    pointerY = ((e.clientY - r.top) / r.height - 0.5) * 0.3
  }
  canvas.addEventListener('pointermove', onPointer)

  const resize = () => {
    const r = canvas.getBoundingClientRect()
    renderer.setSize(Math.max(1, r.width), Math.max(1, r.height), false)
    camera.aspect = Math.max(1, r.width) / Math.max(1, r.height)
    camera.updateProjectionMatrix()
  }
  const ro = new ResizeObserver(resize)
  ro.observe(canvas)
  resize()

  let frame = 0
  const clock = new THREE.Clock()
  const render = () => {
    const t = clock.getElapsedTime()
    avatar.rotation.y += (pointerX - avatar.rotation.y) * 0.045
    avatar.rotation.x += (-pointerY - avatar.rotation.x) * 0.035
    avatar.position.y = Math.sin(t * 1.25) * 0.035
    core.rotation.z = t * 0.9
    renderer.render(scene, camera)
    frame = requestAnimationFrame(render)
  }
  render()

  return () => {
    cancelAnimationFrame(frame)
    ro.disconnect()
    canvas.removeEventListener('pointermove', onPointer)
    renderer.dispose()
  }
}
