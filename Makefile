BOARD=disco_l475_iot1
RUNNER=jlink

flash: build
	west flash -r $(RUNNER) --reset

build:
	west build -b $(BOARD) --pristine
