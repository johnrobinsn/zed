CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g -I../src -I/usr/include/freetype2
CXXFLAGS_COV = $(CXXFLAGS) -fprofile-arcs -ftest-coverage
LDFLAGS = -lX11 -lGL -lGLEW -lfreetype -lm -lpthread
LDFLAGS_COV = $(LDFLAGS) -lgcov --coverage

TESTS = editor_test search_test integration_test file_test utf8_test utf8_click_test
INTEGRATION_TESTS = integration_xvfb_test

all: $(TESTS)
	@echo ""
	@echo "=== Running Editor Tests ==="
	@./editor_test
	@echo ""
	@echo "=== Running Search Tests ==="
	@./search_test
	@echo ""
	@echo "=== Running Integration Tests ==="
	@./integration_test
	@echo ""
	@echo "=== Running File I/O Tests ==="
	@./file_test
	@echo ""
	@echo "=== Running UTF-8 Tests ==="
	@./utf8_test
	@echo ""
	@echo "=== Running UTF-8 Click Positioning Tests ==="
	@./utf8_click_test
	@echo ""
	@echo "✓ All test suites completed!"
	@echo ""
	@echo "Run 'make integration' for Xvfb integration tests (requires xvfb)"
	@echo ""

# Integration tests with real X11 (requires Xvfb)
integration: $(INTEGRATION_TESTS)
	@echo ""
	@echo "=== Running Xvfb Integration Tests ==="
	@./integration_xvfb_test || echo "Some integration tests may have been skipped"
	@echo ""

editor_test: editor_test.cpp test_framework.h
	$(CXX) $(CXXFLAGS) editor_test.cpp -o editor_test $(LDFLAGS)

search_test: search_test.cpp test_framework.h
	$(CXX) $(CXXFLAGS) search_test.cpp -o search_test $(LDFLAGS)

integration_test: integration_test.cpp test_framework.h
	$(CXX) $(CXXFLAGS) integration_test.cpp -o integration_test $(LDFLAGS)

file_test: file_test.cpp test_framework.h
	$(CXX) $(CXXFLAGS) file_test.cpp -o file_test $(LDFLAGS)

utf8_test: utf8_test.cpp test_framework.h test_utilities.h
	$(CXX) $(CXXFLAGS) utf8_test.cpp -o utf8_test $(LDFLAGS)

utf8_click_test: utf8_click_test.cpp test_framework.h test_utilities.h
	$(CXX) $(CXXFLAGS) utf8_click_test.cpp -o utf8_click_test $(LDFLAGS)

integration_xvfb_test: integration_xvfb_test.cpp test_framework.h test_utilities.h
	$(CXX) $(CXXFLAGS) integration_xvfb_test.cpp -o integration_xvfb_test $(LDFLAGS)

# Coverage targets
coverage: clean-coverage
	@echo "Building tests with coverage instrumentation..."
	$(CXX) $(CXXFLAGS_COV) editor_test.cpp -o editor_test_cov $(LDFLAGS_COV)
	$(CXX) $(CXXFLAGS_COV) search_test.cpp -o search_test_cov $(LDFLAGS_COV)
	$(CXX) $(CXXFLAGS_COV) integration_test.cpp -o integration_test_cov $(LDFLAGS_COV)
	@echo "Running tests with coverage..."
	@./editor_test_cov > /dev/null 2>&1 || true
	@./search_test_cov > /dev/null 2>&1 || true
	@./integration_test_cov > /dev/null 2>&1 || true
	@echo "Generating coverage report..."
	@mkdir -p coverage
	@gcov -o . editor_test.cpp search_test.cpp integration_test.cpp > /dev/null 2>&1
	@mv *.gcov coverage/ 2>/dev/null || true
	@echo ""
	@echo "=== Coverage Summary ==="
	@echo "Coverage files generated in coverage/ directory"
	@echo ""
	@$(MAKE) coverage-summary

coverage-summary:
	@echo "Analyzing coverage gaps..."
	@echo ""
	@python3 ../analyze_coverage.py 2>/dev/null || $(MAKE) coverage-summary-basic

coverage-summary-basic:
	@echo "--- Untested Code in editor.h ---"
	@grep -E "^\s*#####:" coverage/editor.h.gcov 2>/dev/null | head -30 || echo "  (File not found or all lines covered)"
	@echo ""
	@echo "--- Untested Code in platform.h ---"
	@grep -E "^\s*#####:" coverage/platform.h.gcov 2>/dev/null | head -30 || echo "  (File not found or all lines covered)"
	@echo ""
	@echo "Legend: ##### = Line never executed"
	@echo ""
	@echo "To view detailed coverage: less coverage/editor.h.gcov"

clean:
	rm -f $(TESTS) $(INTEGRATION_TESTS) *_cov

clean-coverage:
	rm -f *.gcda *.gcno *.gcov
	rm -rf coverage/

.PHONY: all integration clean coverage coverage-summary coverage-summary-basic clean-coverage
