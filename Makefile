# Directory patterns to ignore (space-separated)
IGNORED_PATTERNS := \
	*/managed_components/* \
	*/build/*

# List of source files to format, excluding ignored patterns
SRC_FILES := $(shell find . -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' \) $(foreach pattern,$(IGNORED_PATTERNS),! -path "$(pattern)"))

# Target to format the files
.PHONY: format
format:
	@echo "Formatting the following files:"
	@printf "	%s\n" $(SRC_FILES)
	@clang-format -i $(SRC_FILES)

