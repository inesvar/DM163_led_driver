BOARD=disco_l475_iot1
RUNNER=jlink

flash: build
	west flash -r $(RUNNER) --reset

build: clean
	west build -b $(BOARD) --pristine

clean :
	rm -rf build
