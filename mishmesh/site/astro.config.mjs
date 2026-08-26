import { defineConfig } from 'astro/config';

// build.format: 'file' emits index.html + manual.html at the dist root (not
// /manual/), so the relative links in public/main.js resolve on any static host.
export default defineConfig({
  build: { format: 'file' },
});
