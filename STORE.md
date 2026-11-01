# Rebble Appstore listing — Print Farm Simulator

Copy these fields into [dev-portal.rebble.io](https://dev-portal.rebble.io/submit).

| Field | Value |
| --- | --- |
| App name | Print Farm Simulator |
| Type | Watch app |
| Category | Games |
| Website | *(optional)* |
| Source code | https://github.com/TechJeeper/Print-Farm-Simulator |
| `.pbw` | `build/PrintFarmSimulator.pbw` |

## Description

Paste into each platform asset collection (max 1600 characters):

```
Print Farm Simulator puts a 3D printer farm on your wrist.

Glance at your garage strip: printers earn money on wall-clock time even while the app is closed. When something fails, the watch vibes — open up, clear the jam, reload filament, re-level the bed, or power-cycle a hotend before reputation tanks.

Queue customer jobs (Benchy, Dragon, phone stands, and more). Match filament for better pay. Spend cash on more machines, faster tiers, and staff who auto-fix minor crises. Claim a daily overnight contract for a big bonus.

• Idle progression with persist between opens
• Crises: spaghetti jam, filament out, bed fail, thermal runaway
• Shop: buy up to 6 printers, upgrade Ender → CoreXY → Industrial
• Hire junior/senior staff; unlock exotic filament
• Reputation unlocks tougher materials and bigger orders
• Clay settings: vibe on/off, quiet hours, crisis frequency

Works on Pebble Time 2, Pebble Time, Pebble 2, and Pebble 2 Duo.
```

## Release notes (v1.0.0)

```
Initial release — run a print farm from your wrist. Idle jobs, crisis vibes, shop upgrades, and daily overnight contracts.
```

## Screenshots

Exact display sizes (no bezel). Regenerate with:

```powershell
python tools/generate_store_screenshots.py
```

| Platform | Size | Folder |
| --- | --- | --- |
| basalt (Pebble Time) | 144×168 | `store/screenshots/basalt/` |
| diorite (Pebble 2) | 144×168 | `store/screenshots/diorite/` |
| flint (Pebble 2 Duo) | 144×168 | `store/screenshots/flint/` |
| emery (Pebble Time 2) | 200×228 | `store/screenshots/emery/` |

Scenes (5 each): title, farm strip, jam crisis, filament crisis, shop.

## Listing checklist

- [ ] Large + small app icons
- [x] Screenshots (5 per platform under `store/screenshots/`)
- [ ] Marketing banner (required for watch apps)
- [ ] Description above on each asset collection
- [ ] Publish `build/PrintFarmSimulator.pbw` as release 1.0.0
