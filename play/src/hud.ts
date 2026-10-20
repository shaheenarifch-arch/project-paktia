// Data-driven HUD shared by every episode. Episodes describe what to show
// each frame (HudFrame); the HUD only draws it.

export interface MapMarker { x: number; z: number; color: string; size?: number; shape?: 'dot' | 'square' | 'diamond' | 'ring' }
export interface MapLine { pts: readonly { x: number; z: number }[]; color: string; width?: number; dash?: boolean }

export interface HudFrame {
  eyebrow: string
  title: string
  phase: string
  objective: string
  items: { label: string; done: boolean; progress?: number }[]
  vitals: ({ label: string; bar: number; low?: boolean } | { label: string; value: string; low?: boolean })[]
  hp: number
  mag: number
  magSize: number
  reloading: boolean
  auto: boolean
  abilities: { key: string; name: string; status: string }[]
  status: string | null
  hint?: string | null
  map: {
    bounds: { x0: number; z0: number; x1: number; z1: number }
    lines: MapLine[]
    markers: MapMarker[]
    player: { x: number; z: number; angle: number }
    enemies: { x: number; z: number; confirmed: boolean }[]
    reveal: number
    revealFrom?: { x: number; z: number }
    interference: number
  }
}

export interface CardSpec {
  eyebrow: string
  eyebrowKind?: 'warn' | 'good'
  title: string
  text: string
  stats?: { label: string; value: string }[]
  options?: { label: string; detail: string; action: () => void }[]
  buttons?: { label: string; primary?: boolean; action: () => void }[]
  controls?: boolean
}

const esc = (s: string) => s.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!))

export class Hud {
  private readonly el: HTMLElement
  private readonly q = <T extends HTMLElement>(sel: string) => this.el.querySelector<T>(sel)!
  private readonly map: CanvasRenderingContext2D
  private textT = 0
  private last = ''
  controls = ''

  constructor(root: HTMLElement) {
    this.el = document.createElement('div')
    this.el.className = 'hud'
    this.el.innerHTML = `
      <section class="brief panel">
        <div class="eyebrow"></div>
        <div class="op"></div>
        <div class="phase"></div>
        <p class="objective"></p>
        <ul class="objs"></ul>
      </section>
      <section class="minimap panel">
        <div class="panel-title"><span class="mapname"></span><span class="net"></span></div>
        <canvas width="220" height="220"></canvas>
      </section>
      <section class="vitals panel">
        <div class="row"><span class="lbl">HEALTH</span><div class="bar"><i class="hp"></i></div></div>
        <div class="extra"></div>
      </section>
      <section class="weapon panel">
        <div class="ammo"><strong class="mag"></strong><span class="magsize"></span></div>
        <div class="wname">KR-47 <span class="mode"></span></div>
        <div class="abils"></div>
      </section>
      <div class="hint"></div>
      <div class="toasts"></div>
      <div class="cross"><i></i><i></i><i></i><i></i></div>
      <div class="hitflash"></div>
      <div class="respawn"></div>
      <div class="screen"></div>`
    root.appendChild(this.el)
    this.map = this.q<HTMLCanvasElement>('canvas').getContext('2d')!
  }

  crosshair(x: number, y: number): void { this.q('.cross').style.transform = `translate(${x}px, ${y}px)` }
  hitmarker(): void { const c = this.q('.cross'); c.classList.remove('hit'); void c.offsetWidth; c.classList.add('hit') }
  hit(): void { const h = this.q('.hitflash'); h.classList.remove('on'); void h.offsetWidth; h.classList.add('on') }

  respawn(seconds: number | null): void {
    const r = this.q('.respawn')
    r.style.display = seconds === null ? 'none' : 'block'
    if (seconds !== null) r.innerHTML = `<strong>DOWN</strong><span>Back in ${seconds.toFixed(1)}s</span>`
  }

  toast(text: string, kind: 'info' | 'good' | 'warn' | 'phase', seconds = 3): void {
    const box = this.q('.toasts')
    const t = document.createElement('div')
    t.className = `toast ${kind}`
    t.textContent = text
    box.appendChild(t)
    while (box.children.length > 4) box.firstElementChild?.remove()
    setTimeout(() => { t.classList.add('out'); setTimeout(() => t.remove(), 400) }, seconds * 1000)
  }

  /** A modal card: pause, choices, mission failed or complete. Null hides it. */
  card(spec: CardSpec | null): void {
    const s = this.q('.screen')
    if (!spec) { s.style.display = 'none'; s.innerHTML = ''; this.el.classList.remove('modal'); return }
    this.el.classList.add('modal')
    const stats = spec.stats ? `<div class="stats">${spec.stats.map(x => `<div><b>${esc(x.value)}</b><span>${esc(x.label)}</span></div>`).join('')}</div>` : ''
    const options = spec.options ? `<div class="choices">${spec.options.map((o, i) => `<button class="choice" data-o="${i}"><kbd>${i + 1}</kbd><strong>${esc(o.label)}</strong><span>${esc(o.detail)}</span></button>`).join('')}</div>` : ''
    const buttons = spec.buttons ? `<div class="btns">${spec.buttons.map((b, i) => `<button data-b="${i}" class="${b.primary ? 'primary' : ''}">${esc(b.label)}</button>`).join('')}</div>` : ''
    s.innerHTML = `<div class="card"><div class="eyebrow ${spec.eyebrowKind ?? ''}">${esc(spec.eyebrow)}</div><h2>${esc(spec.title)}</h2>${spec.text ? `<p>${esc(spec.text)}</p>` : ''}${stats}${options}${spec.controls ? this.controls : ''}${buttons}</div>`
    s.style.display = 'grid'
    s.querySelectorAll<HTMLButtonElement>('[data-b]').forEach(b => b.addEventListener('click', () => spec.buttons![Number(b.dataset.b)]!.action()))
    s.querySelectorAll<HTMLButtonElement>('[data-o]').forEach(b => b.addEventListener('click', () => spec.options![Number(b.dataset.o)]!.action()))
  }

  frame(f: HudFrame): void {
    this.drawMap(f)
    if ((this.textT = (this.textT + 1) % 6) !== 0) return
    const key = `${f.eyebrow}|${f.title}|${f.phase}|${f.objective}`
    if (key !== this.last) {
      this.last = key
      this.q('.brief .eyebrow').textContent = f.eyebrow
      this.q('.op').textContent = f.title
      this.q('.mapname').textContent = f.title.replace(/^OPERATION /, '')
      this.q('.phase').textContent = f.phase
      this.q('.objective').textContent = f.objective
    }
    this.q('.objs').innerHTML = f.items.map(o => `<li class="${o.done ? 'done' : ''}"><span>${o.done ? '✓' : '◇'} ${esc(o.label)}</span>${!o.done && o.progress ? `<em>${Math.round(o.progress * 100)}%</em>` : ''}</li>`).join('')
    const hp = this.q('.hp'); hp.style.width = `${Math.round(f.hp * 100)}%`; hp.classList.toggle('low', f.hp < 0.35)
    this.q('.extra').innerHTML = f.vitals.map(v => 'bar' in v
      ? `<div class="row"><span class="lbl">${esc(v.label)}</span><div class="bar"><i class="${v.low ? 'low' : ''}" style="width:${Math.round(Math.max(0, v.bar) * 100)}%"></i></div></div>`
      : `<div class="row"><span class="lbl">${esc(v.label)}</span><strong class="val ${v.low ? 'low' : ''}">${esc(v.value)}</strong></div>`).join('')
    this.q('.mag').textContent = f.reloading ? '--' : String(f.mag)
    this.q('.magsize').textContent = f.reloading ? ' RELOADING' : ` / ${f.magSize}`
    this.q('.mode').textContent = f.auto ? 'AUTO' : 'SINGLE'
    this.q('.abils').innerHTML = f.abilities.map(a => {
      const cls = a.status === 'READY' ? 'ready' : /OFFLINE|LOCKED|NO CREW|—/.test(a.status) ? 'off' : 'wait'
      return `<div class="abil"><kbd>${esc(a.key)}</kbd> ${esc(a.name)} <b class="${cls}">${esc(a.status)}</b></div>`
    }).join('')
    const net = this.q('.net')
    net.textContent = f.status ?? 'NETWORK ONLINE'
    net.classList.toggle('warn', Boolean(f.status))
    const hint = this.q('.hint')
    hint.textContent = f.hint ?? ''
    hint.style.display = f.hint ? 'block' : 'none'
  }

  private drawMap(f: HudFrame): void {
    const c = this.map, S = 220, B = f.map.bounds
    const span = Math.max(B.x1 - B.x0, B.z1 - B.z0), k = S / span
    const ox = (S - (B.x1 - B.x0) * k) / 2, oz = (S - (B.z1 - B.z0) * k) / 2
    const X = (x: number) => ox + (x - B.x0) * k, Z = (z: number) => oz + (z - B.z0) * k
    c.fillStyle = '#0b1512'; c.fillRect(0, 0, S, S)
    c.strokeStyle = 'rgba(126,255,176,.08)'; c.lineWidth = 1
    for (let i = 0; i <= S; i += 22) { c.beginPath(); c.moveTo(i, 0); c.lineTo(i, S); c.moveTo(0, i); c.lineTo(S, i); c.stroke() }
    for (const l of f.map.lines) {
      if (l.pts.length < 2) continue
      c.strokeStyle = l.color; c.lineWidth = (l.width ?? 1) * (l.width && l.width > 3 ? k : 1); c.setLineDash(l.dash ? [3, 3] : [])
      c.beginPath(); l.pts.forEach((p, i) => (i ? c.lineTo(X(p.x), Z(p.z)) : c.moveTo(X(p.x), Z(p.z)))); c.stroke()
    }
    c.setLineDash([]); c.lineWidth = 1
    for (const m of f.map.markers) {
      const s = m.size ?? 3, x = X(m.x), z = Z(m.z)
      c.fillStyle = m.color; c.strokeStyle = m.color
      if (m.shape === 'square') c.fillRect(x - s, z - s, s * 2, s * 2)
      else if (m.shape === 'diamond') { c.beginPath(); c.moveTo(x, z - s - 1); c.lineTo(x + s + 1, z); c.lineTo(x, z + s + 1); c.lineTo(x - s - 1, z); c.closePath(); c.fill() }
      else if (m.shape === 'ring') { c.beginPath(); c.arc(x, z, s, 0, Math.PI * 2); c.stroke() }
      else { c.beginPath(); c.arc(x, z, s, 0, Math.PI * 2); c.fill() }
    }
    const p = f.map.player, from = f.map.revealFrom ?? p
    for (const e of f.map.enemies) {
      if (Math.hypot(e.x - from.x, e.z - from.z) > f.map.reveal) continue
      if (f.map.interference > 0 && Math.random() < f.map.interference) continue
      c.fillStyle = e.confirmed ? '#ff5e63' : 'rgba(255,94,99,.5)'
      c.fillRect(X(e.x) - 1.5, Z(e.z) - 1.5, 3, 3)
    }
    c.save(); c.translate(X(p.x), Z(p.z)); c.rotate(-p.angle + Math.PI)
    c.fillStyle = '#78ffab'; c.beginPath(); c.moveTo(0, -6); c.lineTo(4, 4); c.lineTo(-4, 4); c.closePath(); c.fill(); c.restore()
    c.strokeStyle = 'rgba(120,255,171,.25)'; c.beginPath(); c.arc(X(from.x), Z(from.z), f.map.reveal * k, 0, Math.PI * 2); c.stroke()
    if (f.map.interference > 0) {
      c.fillStyle = `rgba(220,233,227,${0.06 + f.map.interference * 0.2})`
      for (let i = 0; i < 220 * f.map.interference; i++) c.fillRect(Math.random() * S, Math.random() * S, 2, 1)
    }
  }
}

export function controlsHtml(rows: [string[], string][]): string {
  return `<div class="controls">${rows.map(([keys, label]) => `<div>${keys.map(k => `<kbd>${k}</kbd>`).join('')}<span>${label}</span></div>`).join('')}</div>`
}

export const BASE_CONTROLS: [string[], string][] = [
  [['W', 'A', 'S', 'D'], 'Move'], [['Shift'], 'Sprint'], [['Mouse'], 'Aim · click to fire'],
  [['R'], 'Reload'], [['B'], 'Auto / single fire'], [['Esc'], 'Pause'], [['M'], 'Sound on / off']
]
