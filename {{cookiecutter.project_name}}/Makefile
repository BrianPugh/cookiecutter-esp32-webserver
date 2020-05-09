.PHONY: ota


ota:
ifndef ESP32_IP
	$(error ESP32_IP is undefined)
endif
	idf.py build
	curl -X POST ${ESP32_IP}/api/v1/ota --data-binary @- < build/{{cookiecutter.project_name}}.bin
