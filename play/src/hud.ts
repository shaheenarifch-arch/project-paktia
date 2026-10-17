import type { BrokenCitadelMission } from '../../src/index.js'
import { HALF, CELLARS, RIVER } from './world'

export interface HudFrame {
  phase: number | undefined
  objective: string
  objectives: { id: string; label: string; done: boolean; progress: number }[]
  anyOne: boolean
  reinforcements: number
  hp: number
  mag: number
  magSize: number
  reloading: boolean
  auto: boolean
  falcon: string
  aegis: string
  surprise: string | null
  plannedSurprise: string
  map: {
    player: { x: number; z: number; angle: number }
    enemies: { x: number; z: number; confirmed: boolean }[]
    reveal: number
    interference: number
    objectives: { x: number; z: number; state: 'done' | 'open' | 'locked' }[]
    cellars: boolean
    bridgeBlocked: boolean
  }
}

type ScreenKind = 'pause' | 'failed' | 'success'
interface ScreenData { mission: BrokenCitadelMission; kills?: number; deaths?: number; minutes?: number }

const PHASE_NAMES = ['', 'Open the City', 'Break Communications', 'Control Old Gardun', 'Neutralise High Ground', 'Assault the Citadel']
const esc = (s: string) => s.replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!))

export class Hud {
  private readonly el: HTMLElement
  private readonly q = <T extends HTMLElement>(sel: string) => this.el.querySelector<T>(sel)!
  private readonly map: CanvasRenderingContext2D
  private textT = 0
  private lastBrief = ''

  constructor(root: HTMLElement, private readonly actions: { onResume: () => void; onRestore: () => void; onExit: () => void }) {
    this.el = document.createElement('div')
    this.el.className = 'hud'
    this.el.innerHTML = `
      <section class="brief panel">
        <div class="eyebrow">PROJECT PAKTIA // EPISODE 03</div>
        <div class="op">OPERATION BROKEN CITADEL</div>
        <div class="phase"></div>
        <p class="objective"></p>
        <ul class="objs"></ul>
      </section>
      <section class="minimap panel">
        <div class="panel-title"><span>GARDUN</span><span class="net">NETWORK ONLINE</span></div>
        <canvas width="220" height="220"></canvas>
      </section>
      <section class="vitals panel">
        <div class="row"><span class="lbl">HEALTH</span><div class="bar"><i class="hp"></i></div></div>
        <div class="row"><span class="lbl">REINFORCEMENTS</span><strong class="reinf"></strong></div>
      </section>
      <section class="weapon panel">
        <div class="ammo"><strong class="mag"></strong><span class="magsize"></span></div>
        <div class="wname">KR-47 <span class="mode"></span></div>
        <div class="abil"><kbd>F</kbd> FALCON <b class="falcon"></b></div>
        <div class="abil"><kbd>G</kbd> AEGIS <b class="aegis"></b></div>
      </section>
      <div class="toasts"></div>
      <div class="cross"><i></i><i></i><i></i><i></i></div>
      <div class="hitflash"></div>
      <div class="respawn"></div>
      <div class="screen"></div>`
    root.appendChild(this.el)
    this.map = this.q<HTMLCanvasElement>('canvas').getContext('2d')!
  }

  crosshair(x: number, y: number): void { const c = this.q('.cross'); c.style.transform = `translate(${x}px, ${y}px)` }

  hitmarker(): void { const c = this.q('.cross'); c.classList.remove('hit'); void c.offsetWidth; c.classList.add('hit') }

  hit(): void { const h = this.q('.hitflash'); h.classList.remove('on'); void h.offsetWidth; h.classList.add('on') }

  respawn(seconds: number | null): void {
    const r = this.q('.respawn')
    r.style.display = seconds === null ? 'none' : 'block'
    if (seconds !== null) r.innerHTML = `<strong>DOWN</strong><span>Reinforcement deploying in ${seconds.toFixed(1)}s</span>`
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

  screen(kind: ScreenKind | null, data?: ScreenData): void {
    const s = this.q('.screen')
    if (!kind || !data) { s.style.display = 'none'; s.innerHTML = ''; this.el.classList.remove('modal'); return }
    this.el.classList.add('modal')
    const m = data.mission
    const stats = data.kills !== undefined ? `<div class="stats"><div><b>${data.kills}</b><span>hostiles neutralised</span></div><div><b>${data.deaths}</b><span>squads lost</span></div><div><b>${(data.minutes ?? 0).toFixed(1)}</b><span>minutes</span></div></div>` : ''
    const body: Record<ScreenKind, string> = {
      pause: `<div class="eyebrow">PAUSED</div><h2>Operation Broken Citadel</h2>
        <p>Phase ${m.phase ?? '-'} of 5 · ${m.reinforcements} reinforcements left</p>
        ${CONTROLS}
        <div class="btns"><button data-a="resume" class="primary">Resume</button><button data-a="exit">Quit to menu</button></div>`,
      failed: `<div class="eyebrow warn">ASSAULT FAILED</div><h2>Reinforcements exhausted</h2>
        <p>${esc(m.runner.objective)}</p>${stats}
        <div class="btns"><button data-a="restore" class="primary">Restore checkpoint</button><button data-a="exit">Quit to menu</button></div>`,
      success: `<div class="eyebrow good">OPERATION COMPLETE</div><h2>Gardun Citadel secured</h2>
        <p>All five phases taken with ${m.reinforcements} reinforcements to spare.</p>${stats}
        <div class="btns"><button data-a="exit" class="primary">Back to menu</button></div>`
    }
    s.innerHTML = `<div class="card">${body[kind]}</div>`
    s.style.display = 'grid'
    s.querySelectorAll<HTMLButtonElement>('button').forEach(b => b.addEventListener('click', () => {
      const a = b.dataset.a
      if (a === 'resume') this.actions.onResume()
      if (a === 'restore') this.actions.onRestore()
      if (a === 'exit') this.actions.onExit()
    }))
  }

  frame(f: HudFrame): void {
    this.drawMap(f)
    if ((this.textT = (this.textT + 1) % 6) !== 0) return
    const brief = `${f.phase}|${f.objective}|${f.anyOne}`
    if (brief !== this.lastBrief) {
      this.lastBrief = brief
      this.q('.phase').textContent = f.phase ? `PHASE ${f.phase}/5 · ${PHASE_NAMES[f.phase]?.toUpperCase() ?? ''}` : ''
      this.q('.objective').textContent = f.objective + (f.anyOne ? ' (either one)' : '')
    }
    this.q('.objs').innerHTML = f.objectives.map(o => `<li class="${o.done ? 'done' : ''}"><span>${o.done ? '✓' : '◇'} ${esc(o.label)}</span>${!o.done && o.progress > 0 ? `<em>${Math.round(o.progress * 100)}%</em>` : ''}</li>`).join('')
    const hp = this.q('.hp'); hp.style.width = `${Math.round(f.hp * 100)}%`; hp.classList.toggle('low', f.hp < 0.35)
    const reinf = this.q('.reinf'); reinf.textContent = String(f.reinforcements); reinf.classList.toggle('low', f.reinforcements <= 5)
    this.q('.mag').textContent = f.reloading ? '--' : String(f.mag)
    this.q('.magsize').textContent = f.reloading ? ' RELOADING' : ` / ${f.magSize}`
    this.q('.mode').textContent = f.auto ? 'AUTO' : 'SINGLE'
    const ab = (sel: string, v: string) => { const e = this.q(sel); e.textContent = v; e.className = `${sel.slice(1)} ${v === 'READY' ? 'ready' : /OFFLINE|LOCKED|NO CREW/.test(v) ? 'off' : 'wait'}` }
    ab('.falcon', f.falcon); ab('.aegis', f.aegis)
    const net = this.q('.net')
    net.textContent = f.surprise ? f.surprise.replace(/_/g, ' ').toUpperCase() : 'NETWORK ONLINE'
    net.classList.toggle('warn', Boolean(f.surprise))
  }

  private drawMap(f: HudFrame): void {
    const c = this.map, S = 220, k = S / (HALF * 2)
    const X = (x: number) => (x + HALF) * k, Z = (z: number) => (z + HALF) * k
    c.fillStyle = '#0b1512'; c.fillRect(0, 0, S, S)
    c.strokeStyle = 'rgba(126,255,176,.08)'; c.lineWidth = 1
    for (let i = 0; i <= S; i += 22) { c.beginPath(); c.moveTo(i, 0); c.lineTo(i, S); c.moveTo(0, i); c.lineTo(S, i); c.stroke() }
    c.fillStyle = 'rgba(160,130,80,.35)'; c.fillRect(0, Z(RIVER.z0), S, Z(RIVER.z1) - Z(RIVER.z0))
    c.strokeStyle = 'rgba(200,180,140,.35)'; c.strokeRect(X(-72), Z(-301), 144 * k, 141 * k)
    if (f.map.bridgeBlocked) { c.fillStyle = '#ff5e63'; c.fillRect(X(-12), Z(RIVER.z0) - 2, 24 * k, Z(RIVER.z1) - Z(RIVER.z0) + 4) }
    if (f.map.cellars) {
      c.strokeStyle = '#ffd166'; c.setLineDash([3, 3]); c.beginPath(); c.moveTo(X(CELLARS.a.x), Z(CELLARS.a.z)); c.lineTo(X(CELLARS.b.x), Z(CELLARS.b.z)); c.stroke(); c.setLineDash([])
    }
    for (const o of f.map.objectives) {
      c.fillStyle = o.state === 'done' ? '#78ffab' : o.state === 'open' ? '#ffd166' : 'rgba(220,233,227,.25)'
      c.beginPath(); c.arc(X(o.x), Z(o.z), o.state === 'open' ? 4 : 3, 0, Math.PI * 2); c.fill()
    }
    const p = f.map.player
    for (const e of f.map.enemies) {
      if (Math.hypot(e.x - p.x, e.z - p.z) > f.map.reveal) continue
      if (f.map.interference > 0 && Math.random() < f.map.interference) continue
      c.fillStyle = e.confirmed ? '#ff5e63' : 'rgba(255,94,99,.5)'
      c.fillRect(X(e.x) - 1.5, Z(e.z) - 1.5, 3, 3)
    }
    c.save(); c.translate(X(p.x), Z(p.z)); c.rotate(-p.angle + Math.PI)
    c.fillStyle = '#78ffab'; c.beginPath(); c.moveTo(0, -6); c.lineTo(4, 4); c.lineTo(-4, 4); c.closePath(); c.fill(); c.restore()
    c.strokeStyle = 'rgba(120,255,171,.25)'; c.beginPath(); c.arc(X(p.x), Z(p.z), f.map.reveal * k, 0, Math.PI * 2); c.stroke()
    if (f.map.interference > 0) {
      c.fillStyle = `rgba(220,233,227,${0.06 + f.map.interference * 0.2})`
      for (let i = 0; i < 220 * f.map.interference; i++) c.fillRect(Math.random() * S, Math.random() * S, 2, 1)
    }
  }
}

export const CONTROLS = `<div class="controls">
  <div><kbd>W</kbd><kbd>A</kbd><kbd>S</kbd><kbd>D</kbd><span>Move</span></div>
  <div><kbd>Shift</kbd><span>Sprint</span></div>
  <div><kbd>Mouse</kbd><span>Aim · click to fire</span></div>
  <div><kbd>R</kbd><span>Reload</span></div>
  <div><kbd>B</kbd><span>Auto / single fire</span></div>
  <div><kbd>F</kbd><span>Falcon air support at the cursor</span></div>
  <div><kbd>G</kbd><span>Aegis on a confirmed hostile</span></div>
  <div><kbd>E</kbd><span>Use cellar hatch</span></div>
  <div><kbd>Esc</kbd><span>Pause</span></div>
  <div><kbd>M</kbd><span>Sound on / off</span></div>
</div>`
