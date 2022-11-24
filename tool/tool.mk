ifndef ROOT_DIR
$(error should be included by root Makefile)
endif

TOOL_DIR := $(ROOT_DIR)/tool
TOOL_VENV_DIR := $(TOOL_DIR)/venv

.PHONY: tool
tool: | tool-venv

.PHONY: tool-venv
tool-venv: $(TOOL_VENV_DIR)/.env-done

TOOL_MODS := $(dir $(wildcard $(TOOL_DIR)/*/__init__.py))
TOOL_SRCS := $(shell find $(TOOL_MODS) -type f -name '*.py')

$(TOOL_VENV_DIR)/.venv-created:
	python3 -m venv $(TOOL_VENV_DIR)
	touch $@
$(TOOL_VENV_DIR)/.env-done: $(TOOL_SRCS) | $(TOOL_VENV_DIR)/.venv-created
	. $(TOOL_VENV_DIR)/bin/activate && \
		pip install $(TOOL_DIR)
	touch $@

.PHONY: tool-test
tool-test: | tool-venv
	. $(TOOL_VENV_DIR)/bin/activate && \
		python -m unittest discover --start-directory $(TOOL_DIR)

TOOL_DIST_DIR := $(TOOL_DIR)/dist

.PHONY: tool-build
tool-build: $(TOOL_SRCS) | tool-venv
	. $(TOOL_VENV_DIR)/bin/activate && \
		pip install build && \
		python -m build $(TOOL_DIR)

.PHONY: tool-constraints
tool-constraints: $(TOOL_SRCS) | tool-venv
	. $(TOOL_VENV_DIR)/bin/activate && \
		pip freeze --exclude klint > $(TOOL_DIR)/constraints
