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

monitor:
	cd my_esp32_webapp && idf.py monitor

endpt-led: env-test
	# Flashes the LED for a second
	curl -X POST ${ESP32_IP}/api/v1/led/timer --data '{"duration": 1000}'

endpt-system: env-test
	curl ${ESP32_IP}/api/v1/system/info

endpt-upload: env-test
	curl ${ESP32_IP}/api/v1/filesystem/README.md --data-binary @- < README.md

endpt-upload-to-folder: env-test
	curl ${ESP32_IP}/api/v1/filesystem/foo/bar/README.md --data-binary @- < README.md

endpt-delete: env-test endpt-upload
	curl -X DELETE ${ESP32_IP}/api/v1/filesystem/README.md

endpt-delete-folder: env-test endpt-upload-to-folder
	curl -X DELETE ${ESP32_IP}/api/v1/filesystem/foo/

endpt-query-folder: env-test endpt-upload-to-folder
	curl ${ESP32_IP}/api/v1/filesystem/foo/

endpt-nvs-namespace-key: env-test
	curl ${ESP32_IP}/api/v1/nvs/user/key1
	curl ${ESP32_IP}/api/v1/nvs/user/key2

nvs:
	# Generate a flashable bin for the NVS partition
	python3 ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs.csv nvs.bin 0x4000

nvs-flash: nvs
	# Flash the NVS binary
	esptool.py --chip esp32 -p /dev/ttyUSB1 -b 921600 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB 0x9000 nvs.bin 

nvs-update: env-test
	curl -X POST ${ESP32_IP}/api/v1/nvs/user --data '{"key1": 7}'

nvs-get: env-test
	curl ${ESP32_IP}/api/v1/nvs/user

nvs-get-bin: env-test
	curl ${ESP32_IP}/api/v1/nvs/user/key3

size-components: build
	cd my_esp32_webapp && idf.py size-components
