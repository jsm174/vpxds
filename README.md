# VPX Display Server

This is a very small app I put together while working on my Visual Pinball Standalone cabinet.

## Background

I have a 2010 PC with 1060 NVidia graphics card running [Batocera x86_x64](https://mirrors.o2switch.fr/batocera/x86_64/beta/last/).

It has three television screens:

- 46" Sharp Aquos LC46D65U LCD TV
- 30" Philips 30PF9946D LCD TV
- 15" Dynex DX-15L150A11 LCD TV

When the computer is turned on, the ATX power supply turns on a 12v relay, which energizes a surge protector that all three screens are connected to.

After 10 seconds, a [Pinscape](http://mjrnet.org/pinscape/) controller sends IR commands to turn on each tv.

After Batocera finishes launching, EmulationStation displays on the playfield screen.

Next, the [custom.sh](https://github.com/jsm174/vpxds/blob/master/scripts/batocera/custom.sh) script is launched which uses `xrandr` to enable and extend the display to the backglass and dmd tvs:

```bash
   xrandr --output DP-1 --auto --output DP-2 --right-of DP-1 --auto  # enable backglass screen
   xrandr --output DP-5 --auto --right-of DP-2 --auto  # enable dmd screen
```

After the desktop has been extended and the screens are enabled, the `vpxds` app is launched which creates windows for the backglass and dmd.

The settings for these windows, such as enabling and positioning, are controlled by `vpxds.ini`:

```ini
[Settings]

ESURL=

TableWidth=1080
TableHeight=1920

CachePath=/userdata/roms/vpinball/vpxds

Backglass=1
BackglassX=1080
BackglassY=0
BackglassWidth=1024
BackglassHeight=768

DMD=1
DMDX=2104
DMDY=0
DMDWidth=1360
DMDHeight=768
```

When a game is selected in EmulationStation it executes `game-selected` scripts.

I have custom `game-selected.sh` scripts [here](https://github.com/jsm174/vpxds/blob/master/scripts/batocera/emulationstation/scripts/game-selected) that send messages to the `vpxds` app and changes the backglass and dmd images if they exist:

```bash
curl -S "http://127.0.0.1:8111/update?display=backglass&path=/userdata/roms/vpinball/<table>.vpx" > /dev/null 2>&1
curl -S "http://127.0.0.1:8111/update?display=dmd&path=/userdata/roms/vpinball/<table>.vpx" > /dev/null 2>&1
```

For more information on custom EmulationStation scripts, refer to this [wiki](https://wiki.batocera.org/launch_a_script#emulationstation_scripting).

If you would like to clear the backglass and dmd windows, you can use the `reset` command:

```bash
curl -S "http://127.0.0.1:8111/reset" > /dev/null 2>&1
```

## Shoutouts

All the great people at the Batocera team, especially @dmanlfc and @maximumentropy!
