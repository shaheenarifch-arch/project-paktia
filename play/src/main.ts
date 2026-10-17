import './style.css'
import { pickSurpriseEvent, type E02Carryover } from '../../src/index.js'
import mapJson from '../../data/maps/gardun-city.json'
import { Game, EPISODE } from './game'
import { CONTROLS } from './hud'

// Episode 03 reads what Episode 02 handed over. Until Episode 02 is playable,
// the player picks how the convoy arrived.
const CONVOYS: { id: string; title: string; detail: string; carry: E02Carryover }[] = [
  { id: 'full', title: 'Full-strength convoy', detail: 'Crew delivered, both Nomads made it. 30 reinforcements, Falcon air support once the Airfield falls.', carry: { episode: 2, outcome: 'SUCCESS', crewDelivered: true, nomadsRemaining: 2, wardenOperational: true, route: 'MAIN_ROAD', relaysOnline: 1 } },
  { id: 'losses', title: 'Convoy with losses', detail: 'Crew delivered, one Nomad lost. 25 reinforcements, Falcon still available.', carry: { episode: 2, outcome: 'PARTIAL', crewDelivered: true, nomadsRemaining: 1, wardenOperational: true, route: 'RIVERBED', relaysOnline: 1 } },
  { id: 'lean', title: 'Crew lost', detail: 'The Falcon crew did not make it. 20 reinforcements and no air support.', carry: { episode: 2, outcome: 'PARTIAL', crewDelivered: false, nomadsRemaining: 0, wardenOperational: false, route: 'RIVER_DRAINAGE', relaysOnline: 0 } }
]

const app = document.querySelector<HTMLDivElement>('#app')!
const events = (mapJson as { surpriseEvents: string[] }).surpriseEvents

function menu(): void {
  let convoy = 'full'
  let seed = `gardun-${Math.floor(Math.random() * 100000)}`
  app.innerHTML = `
    <div class="menu">
      <div class="menu-card">
        <div class="eyebrow">PROJECT PAKTIA // EPISODE 03 // PLAYABLE PROTOTYPE</div>
        <h1>OPERATION<br>BROKEN CITADEL</h1>
        <p class="lede">${EPISODE.briefing}</p>
        <div class="phases">${['Open the City', 'Break Communications', 'Control Old Gardun', 'Neutralise High Ground', 'Assault the Citadel'].map((n, i) => `<div><b>${i + 1}</b><span>${n}</span></div>`).join('')}</div>
        <h3>How did the Episode 02 convoy arrive?</h3>
        <div class="convoys">${CONVOYS.map(c => `<button class="convoy" data-id="${c.id}"><strong>${c.title}</strong><span>${c.detail}</span></button>`).join('')}</div>
        <div class="seedrow"><span>Match seed <code class="seed"></code></span><span class="hint">One surprise event hits in phase 3. The seed decides which.</span><button class="reroll">New seed</button></div>
        <button class="start primary">Deploy</button>
        <details><summary>Controls</summary>${CONTROLS}</details>
        <p class="fine">Fictional setting, factions and equipment. Desktop browser with keyboard and mouse.</p>
      </div>
    </div>`
  const pick = () => app.querySelectorAll<HTMLButtonElement>('.convoy').forEach(b => b.classList.toggle('on', b.dataset.id === convoy))
  const showSeed = () => { app.querySelector('.seed')!.textContent = seed; app.querySelector<HTMLElement>('.seed')!.title = `Surprise event: ${pickSurpriseEvent(events, seed).replace(/_/g, ' ')}` }
  app.querySelectorAll<HTMLButtonElement>('.convoy').forEach(b => b.addEventListener('click', () => { convoy = b.dataset.id!; pick() }))
  app.querySelector('.reroll')!.addEventListener('click', () => { seed = `gardun-${Math.floor(Math.random() * 100000)}`; showSeed() })
  app.querySelector('.start')!.addEventListener('click', () => start(CONVOYS.find(c => c.id === convoy)!.carry, seed))
  pick(); showSeed()
}

function start(carryover: E02Carryover, seed: string): void {
  app.innerHTML = '<div class="game"></div>'
  const root = app.querySelector<HTMLElement>('.game')!
  try {
    new Game(root, { carryover, seed, onExit: menu })
  } catch (err) {
    app.innerHTML = `<div class="menu"><div class="menu-card"><h2>Could not start</h2><p>${String(err)}</p><button class="primary" onclick="location.reload()">Reload</button></div></div>`
  }
}

menu()
