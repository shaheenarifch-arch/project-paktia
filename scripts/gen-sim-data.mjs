#!/usr/bin/env node
// Generates sim/generated/<name>.inc from data/ so the C++ simulation core
// needs no JSON parser at runtime.
//
// Each section is a module in scripts/sim-data/*.mjs (run in sorted order;
// files starting with "_" are helpers). A section exports:
//   output   name of its file: sim/generated/<output>.inc
//   default  function () => string[] of lines for that file
//
//   node scripts/gen-sim-data.mjs          write all files
//   node scripts/gen-sim-data.mjs --check  fail if any file is out of date (CI)
import { readFileSync, writeFileSync, readdirSync, existsSync, mkdirSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const sectionDir = join(root, 'scripts', 'sim-data')
const outDir = join(root, 'sim', 'generated')
const check = process.argv.includes('--check')

const sections = readdirSync(sectionDir)
  .filter(f => f.endsWith('.mjs') && !f.startsWith('_'))
  .sort()

let stale = 0
for (const file of sections) {
  const mod = await import(pathToFileURL(join(sectionDir, file)).href)
  if (typeof mod.default !== 'function' || typeof mod.output !== 'string') {
    throw new Error(`scripts/sim-data/${file} must export 'output' and a default function`)
  }
  const text = mod.default().join('\n') + '\n'
  const out = join(outDir, `${mod.output}.inc`)
  if (check) {
    const current = existsSync(out) ? readFileSync(out, 'utf8') : ''
    if (current !== text) {
      console.error(`sim/generated/${mod.output}.inc is out of date. Run: npm run sim:gen`)
      stale++
    }
  } else {
    mkdirSync(outDir, { recursive: true })
    writeFileSync(out, text)
    console.log(`wrote ${out} (${file})`)
  }
}
if (check) {
  if (stale > 0) process.exit(1)
  console.log('sim data up to date')
}
