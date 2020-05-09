.PHONY: render build flash ota

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

build: render
	# Builds the default binary
	cd my_esp32_webapp && idf.py build
	
flash: build
	cd my_esp32_webapp && idf.py flash monitor

ota: build
	cd my_esp32_webapp && make ota
