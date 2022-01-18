# Runs bitmap infrastructure tests using test_bitmap kernel module

# No binaries, but make sure arg-less "make" doesn't trigger "run_tests"
all:

run_tests: all
	@if /sbin/modprobe -q test_bitmap; then \
		/sbin/modprobe -q -r test_bitmap; \
		echo "bitmap: ok"; \
	else \
		echo "bitmap: [FAIL]"; \
		exit 1; \
	fi
