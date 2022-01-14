# Copyright (c) 2022 Johannes Stoelp

build:
	pio run

run:
	pio run -e nodemcuv2 -t upload && pio device monitor -b 115200

check:
	pio test

clean:
	pio run -t clean

help:
	@echo "Targets:"
	@echo "  build  - Build project."
	@echo "  run    - Build & flash project and attach serial monitor."
	@echo "  check  - Run tests."
	@echo "  clean  - Clean project."
