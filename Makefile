BOARD=disco_l475_iot1
RUNNER=jlink
DEBUG_PORT=/dev/ttyACM0
# dts file provided by Zephyr
DFT_DT_FILE=../zephyr/boards/arm/disco_l475_iot1/disco_l475_iot1.dts
# final dts file
DT_FILE=build/zephyr/zephyr.dts
# user terminal
MY_TERMINAL=gnome-terminal

flash: build
	west $@ -r $(RUNNER) --reset

build:
	west $@ -b $(BOARD)

clean:
	rm -rf build

debug:
	$(MY_TERMINAL) -- tio $(DEBUG_PORT)

dt:
	$(MY_TERMINAL) -- view $(DT_FILE)

dft-dt:
	$(MY_TERMINAL) -- view $(DFT_DT_FILE)

