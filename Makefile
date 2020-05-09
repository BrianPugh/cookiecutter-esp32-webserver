.PHONY: render env-test build flash ota endpt-led

# All targets in this Makefile are intended for testing if the default
# cookiecutter template compiles and functions properly.

render:
	# Applies the cookiecutter template
	rm -rf my_esp32_webapp
	# Cannot overwrite-if-exist due to how cookiecutter handles `_copy_without_render`
	cookiecutter . --default-config --no-input #--overwrite-if-exists
	# Cache the build directory and sdkconfig between tests
	mkdir -p build
	ln -s ${PWD}/build ${PWD}/my_esp32_webapp/build
	ln -s ${PWD}/sdkconfig ${PWD}/my_esp32_webapp/sdkconfig

env-test:
ifndef ESP32_IP
	$(error ESP32_IP is undefined)
endif

build: render
	# Builds the default binary
	cd my_esp32_webapp && idf.py build
	
flash: build
	cd my_esp32_webapp && idf.py flash monitor

ota: build env-test
	cd my_esp32_webapp && make ota

endpt-led: env-test
	# Flashes the LED for a second
	curl -X POST ${ESP32_IP}/api/v1/led/timer --data '{"duration": 1000}'

endpt-system: env-test
	curl ${ESP32_IP}/api/v1/system/info
