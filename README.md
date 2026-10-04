<p align="center">
  <img src="logo.png" width="160" alt="One Attempt Roulette logo">
</p>

<h1 align="center">One Attempt Roulette</h1>

<p align="center">
  A <b>one-attempt level roulette</b> for Geometry Dash (Geode).<br>
  A random level appears, you get a single attempt, and your percentage is added to the total.
</p>

---

## Download & install

1. Install [Geode](https://geode-sdk.org) (Geometry Dash **2.2081**, **Windows**).
2. Download `verdeancho.percent-roulette.geode` from the [latest release](../../releases/latest).
3. Put the file in your `geode/mods` folder.
   In game: Geode button → **Installed** → folder icon opens it.
4. Restart Geometry Dash. The green **Roulette** button is in the **Create** menu.

## How to play

- **Difficulty**: any rated level from Easy to Extreme Demon (optionally *Force popular levels*).
- **Demonlist**: levels from [Pointercrate](https://pointercrate.com): Main List, Extended, Legacy or any range you type.
- **Specials**: recent levels, random levels from all of GD, friends' levels, gamemode challenges (wave, ship, ball...) and [The Challenge List](https://challengelist.gd).
- Choose **Classic**, **Platformer** or **Both** and how many levels you want, then press **Start**.
- Each level opens its normal info screen (Low Detail, song download...). When you die you leave automatically and the next level loads.
- Practice and restarting are disabled. Quitting counts as your attempt.

Press **?** in the roulette menu for an in-game guide.

## Features

- In-game overlay with progress, total, mean and standard deviation, updating live while you play.
- Stats panel on the level screen with a graph of the roulette.
- Move everything with the mouse and customize size, opacity, background and the color of each value.
- Statistics: best / worst total and most 100% records, full history with dates, level lists, locks and deletion.
- Your roulette is saved automatically, so you can continue it later.
- Optional text files for OBS.

## Credits

- How to play video by **Sr.Guillester** — [watch on YouTube](https://www.youtube.com/watch?v=i_8NfIRx8k8)
- Special thanks to **Jenrai** — [YouTube channel](https://www.youtube.com/@enjoythemomentsbywailai)
- Level lists from [Pointercrate](https://pointercrate.com) and [The Challenge List](https://challengelist.gd).

This mod was developed with the help of AI (Claude).

## Building from source

Requirements: Visual Studio Build Tools 2022, LLVM (clang-cl), CMake, Ninja, [Geode CLI](https://github.com/geode-sdk/cli)
and the Geode SDK (`GEODE_SDK` environment variable).

```
build.bat          # builds and installs the .geode into the GD mods folder
build.bat clean    # full rebuild
```

clang-cl is used because MSVC hits an internal compiler error with Geode 5's async library.
Every push is also built automatically by GitHub Actions (see the *Actions* tab).

Resources: `tools/make_icons.ps1` (roulette icon, logo, eye icons) and `tools/make_credit_icons.ps1`
(credit avatars and video button, generated from `assets/`).

Data is stored in the mod's save folder (`geode/mods/verdeancho.percent-roulette` inside the GD save folder).
