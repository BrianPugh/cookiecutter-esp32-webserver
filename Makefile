.PHONY: test

test:
	rm -rf my_esp32_webapp
	# Cannot overwrite-if-exist due to how cookiecutter handles `_copy_without_render`
	cookiecutter . --default-config --no-input #--overwrite-if-exists
	cd my_esp32_webapp && idf.py flash monitor

