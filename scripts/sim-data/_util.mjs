// Shared helpers for the sim-data generator sections.
import { readFileSync, readdirSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

export const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..')

export const readJson = (...parts) => JSON.parse(readFileSync(join(root, ...parts), 'utf8'))

export const readDir = (dir, key) =>
  readdirSync(join(root, 'data', dir))
    .filter(f => f.endsWith('.json'))
    .sort()
    .flatMap(f => readJson('data', dir, f)[key])

// Numbers are written as integer thousandths ("milli") so the sim can turn
// them into Fixed64 values without floating point.
export const milli = (v, where) => {
  const n = Math.round(Number(v) * 1000)
  if (!Number.isFinite(n)) throw new Error(`Not a number: ${where} = ${v}`)
  return n
}
export const str = s => JSON.stringify(String(s))
// Byte order (not locale order) so the C++ side can binary-search with operator<.
export const byId = (a, b) => (a < b ? -1 : a > b ? 1 : 0)
