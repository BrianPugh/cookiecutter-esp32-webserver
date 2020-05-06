.PHONY: test

test:
	cookiecutter . --output-dir test-project --default-config --no-input --overwrite-if-exists
	cd test-project
	idf.py build
	cd ..

