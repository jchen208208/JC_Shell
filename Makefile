# Thin wrapper over cmake — CMakeLists.txt stays the one build definition.

SHELL_BIN := build/shell
SRC := $(shell find src -type f \( -name '*.c' -o -name '*.h' \))

.PHONY: all run watch clean

all: build/CMakeCache.txt
	cmake --build build

build/CMakeCache.txt:
	cmake -B build -S .

run: all
	./$(SHELL_BIN)

# Rebuilds whenever anything in src/ changes. Ctrl-C to stop.
watch:
	@echo "watching src/ — ctrl-c to stop"
	@last=""; \
	while true; do \
		now=$$(stat -f '%N %m %z' $(SRC) 2>/dev/null); \
		if [ "$$now" != "$$last" ]; then \
			last="$$now"; \
			$(MAKE) --no-print-directory all || true; \
		fi; \
		sleep 1; \
	done

clean:
	rm -rf build
