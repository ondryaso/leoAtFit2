.PHONY: run

VENV_DIR := venv

run: build
	. "$(VENV_DIR)/bin/activate" && python3 ./kry.py -type $(TYPE) -port $(PORT)

build: $(VENV_DIR)

$(VENV_DIR):
	python3 -m virtualenv "$(VENV_DIR)" 1>/dev/null || python3 -m venv "$(VENV_DIR)" 1>/dev/null
	. "$(VENV_DIR)/bin/activate" && pip install -r requirements.txt

