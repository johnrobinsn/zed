#!/usr/bin/env python3
"""
Coverage Gap Analysis for Zed Editor Test Suite
Analyzes gcov output to identify untested code paths and features
"""

import os
import re
from collections import defaultdict

def parse_gcov_file(filepath):
    """Parse a .gcov file and extract untested lines"""
    untested = []
    total_lines = 0
    covered_lines = 0

    try:
        with open(filepath, 'r') as f:
            current_function = None
            for line in f:
                # Match gcov format: execution_count:line_number:source
                match = re.match(r'\s*([^:]+):\s*(\d+):(.*)', line)
                if not match:
                    continue

                count, line_num, source = match.groups()
                source = source.rstrip()

                # Track function names
                if 'inline' in source and '(' in source and '{' not in source:
                    func_match = re.search(r'inline\s+\w+\s+(\w+)\s*\(', source)
                    if func_match:
                        current_function = func_match.group(1)

                # Skip empty lines, comments, braces
                if not source.strip() or source.strip().startswith('//'):
                    continue
                if source.strip() in ['{', '}', '};']:
                    continue

                # Count executable lines
                if count.strip() != '-':
                    total_lines += 1
                    if count.strip() == '#####':
                        untested.append({
                            'line': int(line_num),
                            'source': source,
                            'function': current_function
                        })
                    elif count.strip() not in ['=====', '-']:
                        try:
                            exec_count = int(count.strip())
                            if exec_count > 0:
                                covered_lines += 1
                        except ValueError:
                            pass
    except FileNotFoundError:
        return None, None, []

    coverage_pct = (covered_lines / total_lines * 100) if total_lines > 0 else 0
    return coverage_pct, total_lines, untested

def categorize_gaps(untested_lines):
    """Categorize untested lines by feature"""
    categories = defaultdict(list)

    for item in untested_lines:
        source = item['source'].strip()
        func = item['function']

        # Categorize by function name or code pattern
        if 'clipboard' in source.lower() or func and 'clipboard' in func.lower():
            categories['Clipboard Operations'].append(item)
        elif 'SelectionRequest' in source or 'selection' in source.lower():
            categories['X11 Selection Protocol'].append(item)
        elif 'platform_' in source:
            categories['Platform Layer'].append(item)
        elif 'editor_' in source:
            categories['Editor Logic'].append(item)
        elif 'search' in source.lower():
            categories['Search Features'].append(item)
        elif 'render' in source.lower():
            categories['Rendering'].append(item)
        else:
            categories['Other'].append(item)

    return categories

def main():
    coverage_dir = 'tests/coverage'

    print("=" * 70)
    print("ZED EDITOR - TEST COVERAGE GAP ANALYSIS")
    print("=" * 70)
    print()

    files_to_check = [
        ('editor.h', 'Editor Logic'),
        ('platform.h', 'Platform Layer'),
        ('rope.h', 'Rope Data Structure'),
        ('renderer.h', 'Rendering'),
    ]

    all_gaps = {}

    for filename, component in files_to_check:
        gcov_path = os.path.join(coverage_dir, f'{filename}.gcov')
        coverage_pct, total_lines, untested = parse_gcov_file(gcov_path)

        if coverage_pct is None:
            print(f"⚠️  {component} ({filename}): No coverage data")
            continue

        covered = total_lines - len(untested)
        print(f"📊 {component} ({filename})")
        print(f"   Coverage: {coverage_pct:.1f}% ({covered}/{total_lines} lines)")

        if untested:
            all_gaps[filename] = untested
            print(f"   ⚠️  {len(untested)} untested lines")
        else:
            print(f"   ✅ All lines covered!")
        print()

    # Detailed gap analysis
    if all_gaps:
        print("=" * 70)
        print("DETAILED GAP ANALYSIS")
        print("=" * 70)
        print()

        for filename, gaps in all_gaps.items():
            categories = categorize_gaps(gaps)

            print(f"📁 {filename}")
            print(f"   {len(gaps)} untested lines across {len(categories)} categories")
            print()

            for category, items in sorted(categories.items(), key=lambda x: -len(x[1])):
                print(f"   🔸 {category}: {len(items)} lines")
                for item in items[:5]:  # Show first 5
                    func_info = f" (in {item['function']})" if item['function'] else ""
                    print(f"      Line {item['line']}{func_info}: {item['source'][:60]}")
                if len(items) > 5:
                    print(f"      ... and {len(items) - 5} more")
                print()

        # Recommendations
        print("=" * 70)
        print("RECOMMENDATIONS")
        print("=" * 70)
        print()

        if any('Clipboard' in cat or 'Selection' in cat
               for gaps in all_gaps.values()
               for cat in categorize_gaps(gaps).keys()):
            print("🎯 HIGH PRIORITY: X11 Clipboard Protocol")
            print("   - Current tests use mock clipboard (g_test_clipboard)")
            print("   - Real X11 clipboard code is never executed")
            print("   - Add integration tests that exercise actual X11 clipboard")
            print("   - Consider testing SelectionRequest/SelectionNotify events")
            print()

        if 'platform.h' in all_gaps:
            platform_gaps = categorize_gaps(all_gaps['platform.h'])
            if platform_gaps:
                print("🎯 PLATFORM LAYER GAPS")
                print("   - Platform layer is difficult to test headless")
                print("   - Consider adding mock X11 event injection")
                print("   - Add tests for cursor management, window events")
                print()

        print("💡 GENERAL RECOMMENDATIONS")
        print("   1. Add integration tests for untested features")
        print("   2. Consider property-based testing for rope operations")
        print("   3. Add stress tests for large files and edge cases")
        print("   4. Test error handling paths (file I/O failures, etc.)")
        print()
    else:
        print("🎉 Excellent! All tested code has 100% coverage!")
        print()

    print("=" * 70)
    print("Run 'less tests/coverage/<file>.gcov' to view detailed coverage")
    print("=" * 70)

if __name__ == '__main__':
    main()
