import { defineConfig } from 'vite'

// The prototype imports the gameplay domain layer (../src) and game data
// (../data) directly, so mission rules come from the same code the tests run.
export default defineConfig({
  base: './',
  server: { fs: { allow: ['..'] } },
  build: { chunkSizeWarningLimit: 900 }
})
