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

After the screens are enabled, the `vpxds` app is launched which creates two SDL windows, one for the backglass and one for the dmd. 

The positioning of these screens are controlled by `vpxds.ini`:

```ini
[settings]

displays=2

display1_x=1080
display1_y=0
display1_width=1024
display1_height=768

display2_x=2104
display2_y=0
display2_width=1360
display2_height=768
```

When a game is selected in EmulationStation it executes `game-selected` scripts.

I have custom scripts [here](https://github.com/jsm174/vpxds/blob/master/scripts/batocera/emulationstation/scripts/game-selected/game-selected.sh) and [here](https://github.com/jsm174/vpxds/blob/master/scripts/batocera/vpinball/game-selected.sh) that send messages to the `vpxds` app and changes the backglass and dmd images if they exist:

```bash
curl -S "http://127.0.0.1:8111/update?display=1&image=/userdata/roms/vpinball/<table>-backglass.png" > /dev/null 2>&1
curl -S "http://127.0.0.1:8111/update?display=2&image=/userdata/roms/vpinball/<table>-dmd.png" > /dev/null 2>&1
```

For more information on custom EmulationStation scripts, refer to this [wiki](https://wiki.batocera.org/launch_a_script#emulationstation_scripting).
