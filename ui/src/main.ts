import './style.css'

document.querySelector<HTMLDivElement>('#app')!.innerHTML = `
<div class="shell">

  <header>
    <div>
      <div class="eyebrow">PROJECT PAKTIA // TACTICAL COMMAND</div>
      <h1>OPERATION MOUNTAIN SHIELD</h1>
    </div>
    <div class="mission">
      <span class="live"></span> LIVE
      <strong>VIPER-1</strong>
    </div>
  </header>

  <main>

    <section class="map panel">
      <div class="panel-title">
        <span>TACTICAL MAP</span>
        <span class="status">NETWORK ONLINE</span>
      </div>

      <div class="terrain">
        <div class="contour c1"></div>
        <div class="contour c2"></div>
        <div class="route"></div>

        <button class="marker ridge" data-target="RIDGE POSITION 03">
          ▲
          <small>RIDGE</small>
        </button>

        <button class="marker contact hostile" data-target="CONTACT-03">
          ●
          <small>CONTACT</small>
        </button>

        <button class="marker viper">
          ◆
          <small>VIPER</small>
        </button>

        <button class="marker echo">
          ■
          <small>ECHO</small>
        </button>

        <button class="marker convoy">
          ▣
          <small>CONVOY</small>
        </button>
      </div>

      <div class="map-tabs">
        <button class="tab active">TACTICAL</button>
        <button class="tab">DRONES</button>
        <button class="tab">SQUADS</button>
        <button class="tab">SUPPORT</button>
      </div>
    </section>

    <section class="right">

      <div class="drone panel">
        <div class="panel-title">
          <span>RAVEN-1 // RECON</span>
          <span class="status">ACTIVE</span>
        </div>

        <div class="telemetry">
          <span>SIGNAL <strong id="signal">91%</strong></span>
          <span>BATTERY <strong id="battery">67%</strong></span>
          <span>ALT <strong>184 M</strong></span>
        </div>

        <div class="camera">
          <div class="scanline"></div>
          <div class="reticle">
            <i></i><i></i><i></i><i></i>
          </div>

          <div class="camera-label">
            RAVEN OPTICAL FEED<br>
            <span>TRACKING ENABLED</span>
          </div>
        </div>

        <div class="controls">
          <button>MOVE</button>
          <button>ORBIT</button>
          <button>TRACK</button>
          <button id="returnDrone">RETURN</button>
        </div>
      </div>

      <div class="contact-card panel">
        <div>
          <span class="eyebrow">SELECTED CONTACT</span>
          <h2 id="targetName">CONTACT-03</h2>
          <p>Classification: ARMORED GAME UNIT</p>
        </div>

        <div class="confirmed">
          CONFIRMED<br>
          <small>RAVEN-1</small>
        </div>
      </div>

      <div class="aegis panel">
        <div class="panel-title">
          <span>AEGIS TACTICAL SUPPORT</span>
          <span class="aegis-online">ONLINE</span>
        </div>

        <div class="target-data">
          <span>TARGET</span>
          <strong id="aegisTarget">CONTACT-03</strong>

          <span>TRACK</span>
          <strong class="green">ACTIVE</strong>

          <span>VALIDATION</span>
          <strong class="green">READY</strong>
        </div>

        <button id="authorize" class="authorize">
          <span>HOLD TO AUTHORIZE</span>
          <div id="progress"></div>
        </button>

        <div class="support-actions">
          <button id="abort" class="danger">ABORT</button>
          <span id="supportState">AWAITING AUTHORIZATION</span>
        </div>
      </div>

    </section>

  </main>

  <footer class="assets">
    <div><span>VIPER</span><strong>READY</strong></div>
    <div><span>BRAVO</span><strong>HOLDING</strong></div>
    <div><span>RAVEN</span><strong class="green">ACTIVE</strong></div>
    <div><span>WARDEN</span><strong>ORBITING</strong></div>
    <div><span>TALON</span><strong>AVAILABLE</strong></div>
    <div class="objective">
      <span>CURRENT OBJECTIVE</span>
      <strong>DEFEND CHECKPOINT ECHO</strong>
    </div>
  </footer>

</div>
`

const targetName = document.querySelector('#targetName')!
const aegisTarget = document.querySelector('#aegisTarget')!

document.querySelectorAll<HTMLButtonElement>('.marker[data-target]').forEach(marker => {
  marker.onclick = () => {
    const target = marker.dataset.target || 'UNKNOWN'
    targetName.textContent = target
    aegisTarget.textContent = target
  }
})

const authorize = document.querySelector<HTMLButtonElement>('#authorize')!
const progress = document.querySelector<HTMLDivElement>('#progress')!
const supportState = document.querySelector('#supportState')!

let timer: number | undefined

const beginAuthorization = () => {
  progress.style.transition = 'width 1.5s linear'
  progress.style.width = '100%'

  timer = window.setTimeout(() => {
    supportState.textContent = 'SUPPORT AUTHORIZED'
    supportState.classList.add('authorized')
    authorize.querySelector('span')!.textContent = 'AUTHORIZED'
  }, 1500)
}

const cancelAuthorization = () => {
  if (timer) clearTimeout(timer)

  if (!supportState.classList.contains('authorized')) {
    progress.style.transition = 'width .15s ease'
    progress.style.width = '0'
  }
}

authorize.addEventListener('pointerdown', beginAuthorization)
authorize.addEventListener('pointerup', cancelAuthorization)
authorize.addEventListener('pointerleave', cancelAuthorization)

document.querySelector('#abort')!.addEventListener('click', () => {
  if (timer) clearTimeout(timer)

  progress.style.transition = 'none'
  progress.style.width = '0'

  supportState.textContent = 'AUTHORIZATION ABORTED'
  supportState.classList.remove('authorized')

  authorize.querySelector('span')!.textContent = 'HOLD TO AUTHORIZE'
})

document.querySelector('#returnDrone')!.addEventListener('click', e => {
  const button = e.currentTarget as HTMLButtonElement
  button.textContent = 'RETURNING'
  document.querySelector('#signal')!.textContent = '88%'
})
