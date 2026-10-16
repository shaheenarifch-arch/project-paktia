import test from 'node:test'
import assert from 'node:assert/strict'
import { AegisSupportController, ContactManager, DroneController, MountainShieldMission, ProtectedZone, RavenDrone } from '../index.js'

test('Raven launches and reports telemetry', () => {
  const drones = new DroneController()
  drones.register(new RavenDrone())
  assert.equal(drones.launch('raven-1', { x: 0, y: 0, z: 0 }), true)
  assert.equal(drones.get('raven-1')?.state, 'ACTIVE')
})

test('Aegis accepts only confirmed, observed, non-protected game targets', () => {
  const contacts = new ContactManager()
  contacts.upsert({ id: 'game-target-1', classification: 'ARMORED_GAME_UNIT', position: { x: 100, y: 0, z: 100 }, source: 'RAVEN', time: 1, quality: 1 })
  contacts.confirm('game-target-1')
  const aegis = new AegisSupportController(contacts)
  aegis.setOnline(true)
  assert.deepEqual(aegis.selectTarget('game-target-1'), { valid: true })
  assert.deepEqual(aegis.validateSelection(), { valid: true })
  assert.ok(aegis.authorize(2))
})

test('Protected zone blocks Aegis authorization', () => {
  const contacts = new ContactManager()
  contacts.upsert({ id: 'game-target-2', classification: 'GAME_UNIT', position: { x: 10, y: 0, z: 10 }, source: 'WARDEN', time: 1, quality: 1 })
  contacts.confirm('game-target-2')
  const aegis = new AegisSupportController(contacts)
  aegis.setProtectedZones([new ProtectedZone('friendly-base', 'FRIENDLY_BASE', { x: 0, y: 0, z: 0 }, 50)])
  aegis.setOnline(true)
  assert.deepEqual(aegis.selectTarget('game-target-2'), { valid: false, reason: 'PROTECTED_ZONE' })
})

test('Capturing ridge station enables Aegis', () => {
  const contacts = new ContactManager()
  const drones = new DroneController()
  const aegis = new AegisSupportController(contacts)
  const mission = new MountainShieldMission(drones, aegis)
  mission.transition('M01_RIDGE_STATION_CAPTURED')
  assert.equal(mission.ridgeStation.state, 'CAPTURED')
  assert.equal(aegis.state, 'AVAILABLE')
})
