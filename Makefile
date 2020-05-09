.PHONY: test

test:
	rm -rf my_esp32_webapp
	# Cannot overwrite-if-exist due to how cookiecutter handles `_copy_without_render`
	cookiecutter . --default-config --no-input #--overwrite-if-exists
	# Cache the build directory and sdkconfig between tests
	mkdir -p build
	ln -s ${PWD}/build ${PWD}/my_esp32_webapp/build
	ln -s ${PWD}/sdkconfig ${PWD}/my_esp32_webapp/sdkconfig
	cd my_esp32_webapp && idf.py flash monitor

