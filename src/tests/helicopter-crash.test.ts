import test from 'node:test'
import assert from 'node:assert/strict'
import { ContactManager } from '../gameplay/intelligence/ContactManager.js'
import { AegisSupportController } from '../gameplay/support/AegisSupportController.js'
import { DroneController } from '../gameplay/drones/DroneController.js'
import { MountainShieldMission } from '../gameplay/missions/mission01/MountainShield.js'

test('Mission 01 completes the Falcon-2 crash recovery sequence', () => {
  const mission = new MountainShieldMission(
    new DroneController(),
    new AegisSupportController(new ContactManager())
  )
  mission.transition('M01_COUNTERATTACK')
  for (let i = 0; i < 10; i += 1) mission.continueCrashSequence()
  assert.equal(mission.state, 'M01_EVAC_COMPLETE')
  assert.equal(mission.helicopterCrash.controller.state, 'EVACUATION_COMPLETE')
  assert.equal(mission.helicopterCrash.controller.crew.recoveredCount, 3)
  mission.continueCrashSequence()
  assert.equal(mission.state, 'M01_FINAL_DRONE_DEFENSE')
})
