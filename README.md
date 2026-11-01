# Print Farm Simulator

Manage a 3D printer farm from your Pebble wrist. Hybrid idle tycoon + crisis manager: printers earn money while you glance, and the watch vibes when something fails.

## Play

- **SELECT** — open farm / open printer / confirm action (cut jam, load filament, buy)
- **UP / DOWN** — move focus, pick filament, scrap a failed job (DOWN on detail)
- **BACK** — leave detail/shop; from title exits the app
- Shop: buy printers (up to 6), upgrade tiers (Ender → CoreXY → Industrial), hire staff, unlock exotic filament
- Daily overnight contract auto-assigns once per day to an idle printer

## Build (Windows)

```powershell
npm install
./build.ps1
```

Optional: `./build.ps1 -Phone 192.168.x.x` to install, or `-Zip` for a CloudPebble import.

Targets: basalt, diorite, flint, emery (rectangular Pebble Time / Pebble 2 / Time 2).

## Settings (phone)

Vibrate on/off, quiet hours (10pm–7am), crisis frequency (Low / Normal / Chaotic).
