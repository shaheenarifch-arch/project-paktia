// Tiny synthesised sound set (no audio files): rifle shots, impacts, chimes.
export class Sound {
  private ctx: AudioContext | null = null
  private noise: AudioBuffer | null = null
  private on = true

  unlock(): void {
    if (this.ctx) { if (this.ctx.state === 'suspended') void this.ctx.resume(); return }
    try {
      this.ctx = new AudioContext()
      const len = this.ctx.sampleRate * 0.5
      this.noise = this.ctx.createBuffer(1, len, this.ctx.sampleRate)
      const d = this.noise.getChannelData(0)
      for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1
    } catch { this.ctx = null }
  }

  toggle(): boolean { this.on = !this.on; return this.on }

  private burst(volume: number, duration: number, freq: number, q = 0.8): void {
    if (!this.on || !this.ctx || !this.noise) return
    const src = this.ctx.createBufferSource(); src.buffer = this.noise
    const filter = this.ctx.createBiquadFilter(); filter.type = 'bandpass'; filter.frequency.value = freq; filter.Q.value = q
    const gain = this.ctx.createGain()
    const t = this.ctx.currentTime
    gain.gain.setValueAtTime(volume, t); gain.gain.exponentialRampToValueAtTime(0.0008, t + duration)
    src.connect(filter).connect(gain).connect(this.ctx.destination)
    src.start(t); src.stop(t + duration)
  }

  shot(volume: number): void { this.burst(0.35 * volume, 0.09, 1400) }
  boom(): void { this.burst(0.6, 0.7, 180, 0.5) }
  click(): void { this.burst(0.15, 0.03, 3000, 2) }

  chime(up: boolean): void {
    if (!this.on || !this.ctx) return
    const t = this.ctx.currentTime
    ;(up ? [523, 784] : [440, 330]).forEach((f, i) => {
      const o = this.ctx!.createOscillator(), g = this.ctx!.createGain()
      o.type = 'triangle'; o.frequency.value = f
      g.gain.setValueAtTime(0.0001, t + i * 0.12); g.gain.exponentialRampToValueAtTime(0.12, t + i * 0.12 + 0.02); g.gain.exponentialRampToValueAtTime(0.0001, t + i * 0.12 + 0.35)
      o.connect(g).connect(this.ctx!.destination); o.start(t + i * 0.12); o.stop(t + i * 0.12 + 0.4)
    })
  }
}
