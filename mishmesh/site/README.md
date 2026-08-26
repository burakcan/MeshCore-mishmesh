# mishmesh site

Marketing + manual site for mishmesh, built with [Astro](https://astro.build).

## Develop

    npm install
    npm run dev        # http://localhost:4321
    npm run build      # static output -> dist/
    npm run preview    # serve the built dist/

## Structure

    src/pages/index.astro     landing (hero, what-it-is, apps, screens, install)
    src/pages/manual.astro    manual, built from components below
    src/layouts/Base.astro    <head>, favicon, theme, loads /main.js
    src/components/            StatusBar, Screenshot, ShotRow, Tip (reused across the manual)
    src/styles/               landing.css / manual.css (shared design tokens)
    public/main.js            theme toggle, install tabs, copy, scrollspy, link wiring
    public/assets/nokiafc22.ttf   the device's real font (Nokia Cellphone FC)
    public/assets/img/*.png       screenshots from tools/mishmesh-shots

The repo/releases URL is the `REPO` constant near the top of `public/main.js`.
`astro.config.mjs` sets `build.format: 'file'` so pages emit as `index.html` /
`manual.html` (not `/manual/`), keeping the relative links host-agnostic.

## Deploy

`npm run build`, then publish `dist/` on any static host (GitHub Pages, Netlify,
...). For GitHub Pages serving under a repo subpath, set `base` in
`astro.config.mjs`.

## Updating screenshots

Regenerate with the (gitignored) shots tool, then copy the PNGs in:

    sh tools/mishmesh-shots/build.sh && tools/mishmesh-shots/shots
    sh tools/mishmesh-shots/topng.sh
    cp tools/mishmesh-shots/out/<name>.png mishmesh/site/public/assets/img/
