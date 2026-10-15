import { HelicopterController } from '../../aircraft/HelicopterController.js'
import { HelicopterCrashController } from '../../aircraft/HelicopterCrashController.js'

export class HelicopterCrashEvent {
  readonly helicopter = new HelicopterController(
    'falcon-2',
    'FALCON-2'
  )

  readonly controller = new HelicopterCrashController(
    this.helicopter
  )

  start(): void {
    this.controller.beginInbound({
      x: 920,
      y: 210,
      z: 680
    })
  }

  triggerDistress(): void {
    this.controller.triggerDistress()
  }

  loseSignal(): void {
    this.controller.loseSignal()
  }

  triggerCrash(): void {
    this.controller.registerCrash({
      x: 740,
      y: 22,
      z: 910
    })
  }

  beginRecon(): void {
    this.controller.beginSearch()
  }

  confirmCrashSite(): void {
    this.controller.locateCrashSite({
      x: 740,
      y: 22,
      z: 910
    })
  }

  secureSite(): void {
    this.controller.secureCrashSite()
  }

  beginRecovery(): void {
    this.controller.beginCrewRecovery()

    this.controller.crew.locate(
      'falcon-pilot',
      'INJURED'
    )

    this.controller.crew.locate(
      'falcon-copilot',
      'CRITICAL'
    )

    this.controller.crew.locate(
      'falcon-chief',
      'OPERATIONAL'
    )
  }

  beginDefense(): void {
    this.controller.startRecoveryDefense()
  }

  evacuateCrew(): boolean {
    this.controller.crew.recover(
      'falcon-pilot'
    )

    this.controller.crew.recover(
      'falcon-copilot'
    )

    this.controller.crew.recover(
      'falcon-chief'
    )

    /*
     * Recovery defense leaves the crash site contested.
     * Secure it again before completing evacuation.
     */
    this.controller.secureCrashSite()

    return this.controller.completeEvacuation()
  }

  abandonRecovery(): void {
    this.controller.abandon()
  }

  snapshot(): Record<string, unknown> {
    return this.controller.snapshot()
  }
}
