import './style.css'
import { CAMPAIGN, pickSurpriseEvent, type E02Carryover } from '../../src/index.js'
import mapJson from '../../data/maps/gardun-city.json'
import { SilentCorridorGame } from './episode02'
import { BrokenCitadelGame } from './episode03'
import { MountainShieldGame } from './episode01'
import { OpenStraitGame } from './episode04'
import { BASE_CONTROLS, controlsHtml } from './hud'
import { loadCampaign, loadCrewRecovered, recordEpisode01, recordEpisode02, recordEpisode03, recordEpisode04, resetCampaign, saveCrewRecovered } from './campaign'

// Episode 03 without an Episode 02 result yet: the player picks how the convoy arrived.
const CONVOYS: { id: string; title: string; detail: string; carry: E02Carryover }[] = [
  { id: 'full', title: 'Full-strength convoy', detail: 'Crew delivered, both Nomads. 30 reinforcements and Falcon air support.', carry: { episode: 2, outcome: 'SUCCESS', crewDelivered: true, nomadsRemaining: 2, wardenOperational: true, route: 'MAIN_ROAD', relaysOnline: 1 } },
  { id: 'losses', title: 'Convoy with losses', detail: 'Crew delivered, one Nomad lost. 25 reinforcements, Falcon available.', carry: { episode: 2, outcome: 'PARTIAL', crewDelivered: true, nomadsRemaining: 1, wardenOperational: true, route: 'RIVERBED', relaysOnline: 1 } },
  { id: 'lean', title: 'Crew lost', detail: 'No Falcon crew. 20 reinforcements and no air support.', carry: { episode: 2, outcome: 'PARTIAL', crewDelivered: false, nomadsRemaining: 0, wardenOperational: false, route: 'RIVER_DRAINAGE', relaysOnline: 0 } }
]
const SUMMARY: Record<number, string> = {
  1: 'Learn the Raven drone, hold Checkpoint Echo, set up Ridge Station, then recover the Falcon-2 crew (or abandon them) and survive the counterattack.',
  2: 'Escort the Falcon-2 crew and supplies down the valley to Gardun\'s South Gate. Fly Raven, place Relay Kilo, choose your way past the blocked bridge.',
  3: 'Take Gardun in five Stronghold Assault phases, from the Airfield to the Citadel\'s Operations Room.',
  4: 'Hypothetical scenario on real geography. Land on Kharg Island, secure its oil terminal intact, clear the shipping lanes and escort the tankers through the strait.'
}

const app = document.querySelector<HTMLDivElement>('#app')!
const events = (mapJson as { surpriseEvents: string[] }).surpriseEvents
let campaign = loadCampaign()

const outcomeChip = (o: string | undefined) => o ? `<span class="chip ${o.toLowerCase()}">${o === 'SUCCESS' ? 'Complete' : o === 'PARTIAL' ? 'Complete with losses' : 'Failed'}</span>` : ''

function menu(): void {
  let convoy = 'full'
  let seed = `gardun-${Math.floor(Math.random() * 100000)}`
  const e01 = campaign.outcome(1)
  let crew = e01 ? campaign.crewRecoveredInMission01() : loadCrewRecovered()
  const e02 = campaign.episode02()
  const e02Passed = e02 && (e02.outcome === 'SUCCESS' || e02.outcome === 'PARTIAL')
  app.innerHTML = `
    <div class="menu">
      <div class="menu-card">
        <div class="eyebrow">OPERATION MIDNIGHT HAVOC // CAMPAIGN // PLAYABLE PROTOTYPE</div>
        <h1>GARDUN<br>CAMPAIGN</h1>
        <p class="lede">Each episode hands its result to the next. Progress is saved in this browser.</p>
        <div class="episodes">
          ${CAMPAIGN.map(ep => `
            <section class="episode " data-ep="${ep.episode}">
              <div class="ep-head"><b>EPISODE ${String(ep.episode).padStart(2, '0')}</b>${outcomeChip(campaign.outcome(ep.episode))}${ep.episode === 4 ? '<span class="chip">Hypothetical scenario</span>' : ''}</div>
              <h3>${ep.name}</h3>
              <p>${SUMMARY[ep.episode] ?? ''}</p>
              ${ep.episode === 1 ? `
                <button class="primary play" data-play="1">${e01 ? 'Replay' : 'Play'} Episode 01</button>
                ${e01 ? '' : `<div class="note">Or skip it and say how it ended (decides what Nomad-1 carries):</div>
                <div class="toggle"><button class="crew ${crew ? 'on' : ''}" data-crew="1">Crew recovered</button><button class="crew ${crew ? '' : 'on'}" data-crew="0">Crash recovery abandoned</button></div>`}` : ''}
              ${ep.episode === 2 ? `<button class="primary play" data-play="2">${campaign.outcome(2) ? 'Replay' : 'Play'} Episode 02</button>` : ''}
              ${ep.episode === 3 ? (e02Passed ? `
                <div class="note">From Episode 02: ${e02!.crewDelivered ? 'crew delivered' : 'no crew'}, ${e02!.nomadsRemaining} Nomad${e02!.nomadsRemaining === 1 ? '' : 's'}. ${20 + 5 * e02!.nomadsRemaining} reinforcements${e02!.crewDelivered ? ', Falcon air support' : ''}.</div>` : `
                <div class="note">No Episode 02 result yet. Play it first, or pick how the convoy arrived:</div>
                <div class="convoys">${CONVOYS.map(c => `<button class="convoy" data-id="${c.id}"><strong>${c.title}</strong><span>${c.detail}</span></button>`).join('')}</div>`) + `
                <div class="seedrow"><span>Match seed <code class="seed"></code></span><button class="reroll">New seed</button></div>
                <button class="primary play" data-play="3">${campaign.outcome(3) ? 'Replay' : 'Play'} Episode 03</button>` : ''}
              ${ep.episode === 4 ? `
                <div class="note">${campaign.outcome(3) === 'SUCCESS' ? 'Episode 03 won: 10 bonus reinforcements.' : 'Win Episode 03 for 10 bonus reinforcements.'} Fictional kit; game-balance ratings only.</div>
                <button class="primary play" data-play="4">${campaign.outcome(4) ? 'Replay' : 'Play'} Episode 04</button>` : ''}
            </section>`).join('')}
        </div>
        <div class="menu-foot">
          <details><summary>Controls</summary>${controlsHtml([...BASE_CONTROLS, [['E'], 'Place relay · use hatch'], [['Q'], 'Episodes 01, 02, 04: fly Raven'], [['F'], 'Episodes 03, 04: Falcon air support'], [['G'], 'Episode 03: Aegis']])}</details>
          <button class="reset">Reset campaign</button>
        </div>
        <p class="fine">Fictional setting, factions and equipment. Desktop browser with keyboard and mouse.</p>
      </div>
    </div>`
  const pick = () => app.querySelectorAll<HTMLButtonElement>('.convoy').forEach(b => b.classList.toggle('on', b.dataset.id === convoy))
  const showSeed = () => { const s = app.querySelector<HTMLElement>('.seed'); if (s) { s.textContent = seed; s.title = `Surprise event: ${pickSurpriseEvent(events, seed).replace(/_/g, ' ')}` } }
  app.querySelectorAll<HTMLButtonElement>('.convoy').forEach(b => b.addEventListener('click', () => { convoy = b.dataset.id!; pick() }))
  app.querySelectorAll<HTMLButtonElement>('.crew').forEach(b => b.addEventListener('click', () => {
    crew = b.dataset.crew === '1'; saveCrewRecovered(crew)
    app.querySelectorAll<HTMLButtonElement>('.crew').forEach(x => x.classList.toggle('on', (x.dataset.crew === '1') === crew))
  }))
  app.querySelector('.reroll')?.addEventListener('click', () => { seed = `gardun-${Math.floor(Math.random() * 100000)}`; showSeed() })
  app.querySelector('.reset')!.addEventListener('click', () => { campaign = resetCampaign(); menu() })
  app.querySelector('[data-play="1"]')!.addEventListener('click', playEpisode01)
  app.querySelector('[data-play="4"]')!.addEventListener('click', playEpisode04)
  app.querySelector('[data-play="2"]')!.addEventListener('click', () => playEpisode02(crew))
  app.querySelector('[data-play="3"]')!.addEventListener('click', () => playEpisode03(e02Passed ? e02 : CONVOYS.find(c => c.id === convoy)!.carry, seed))
  pick(); showSeed()
}

function mount(): HTMLElement {
  app.innerHTML = '<div class="game"></div>'
  return app.querySelector<HTMLElement>('.game')!
}

function fail(err: unknown): void {
  app.innerHTML = `<div class="menu"><div class="menu-card"><h2>Could not start</h2><p>${String(err)}</p><button class="primary" id="back">Back to menu</button></div></div>`
  app.querySelector('#back')!.addEventListener('click', menu)
}

function playEpisode01(): void {
  try {
    new MountainShieldGame(mount(), { onResult: (outcome, carry) => recordEpisode01(campaign, outcome, carry), onExit: menu })
  } catch (err) { fail(err) }
}

function playEpisode04(): void {
  try {
    new OpenStraitGame(mount(), { episode3: campaign.outcome(3), onResult: outcome => recordEpisode04(campaign, outcome), onExit: menu })
  } catch (err) { fail(err) }
}

function playEpisode02(crewRecovered: boolean): void {
  try {
    new SilentCorridorGame(mount(), {
      crewRecovered,
      onResult: (outcome, carryover) => recordEpisode02(campaign, outcome, carryover, crewRecovered),
      onExit: menu
    })
  } catch (err) { fail(err) }
}

function playEpisode03(carryover: E02Carryover | undefined, seed: string): void {
  try {
    new BrokenCitadelGame(mount(), { carryover, seed, onResult: outcome => recordEpisode03(campaign, outcome), onExit: menu })
  } catch (err) { fail(err) }
}

menu()
