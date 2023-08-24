#!/bin/bash

case "$1" in
    start)
        sleep 1

        export DISPLAY=:0
        export PATH=.:$PATH

        xrandr --output DP-1 --auto --output DP-2 --right-of DP-1 --auto
        xrandr --output DP-5 --auto --right-of DP-2 --auto

        sleep 1

        cd /userdata/system/configs/vpinball

        vpxds &

        ;;
esac

exit $?
