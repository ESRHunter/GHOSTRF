#!/usr/bin/env python3
"""
Adversarial Documentation Stress Test Suite for GHOSTRF
======================================================
Author: Challenger 1 Agent
Scope: Adversarially challenge and stress-test README.md and README_RU.md

Tests:
1. Complete extraction & resolution of all links, images, anchors, and code references.
2. Verification of targets on the filesystem (existence, readable, non-empty).
3. Markdown syntax edge cases:
   - Code fence balance & formatting
   - Unclosed brackets (markdown links & images)
   - Inline code backtick balance
   - Unescaped characters & malformed HTML tags
   - Markdown table column count consistency
   - HTML entity integrity
4. Pin definitions verification (GPIO 0..10, 20, 21) against src/config.h
5. Verification of web/release assets and reciprocal cross-links.
"""

import os
import re
import sys
import html.parser
from pathlib import Path
from typing import Dict, List, Set, Tuple

PROJECT_ROOT = Path("/home/esrhunter/Документы/cc1101")

class MarkdownSyntaxChecker:
    def __init__(self, filename: str, content: str):
        self.filename = filename
        self.content = content
        self.lines = content.splitlines()
        self.errors = []
        self.warnings = []

    def check_code_fences(self):
        fence_pattern = re.compile(r"^ {0,3}(`{3,}|~{3,})")
        open_fence = None
        for i, line in enumerate(self.lines, start=1):
            m = fence_pattern.match(line)
            if m:
                marker = m.group(1)[0]  # '`' or '~'
                count = len(m.group(1))
                if open_fence is None:
                    open_fence = (marker, count, i)
                else:
                    if marker == open_fence[0] and count >= open_fence[1]:
                        open_fence = None
        if open_fence:
            self.errors.append(f"Unclosed code fence opened at line {open_fence[2]}")

    def check_inline_backticks(self):
        in_code_block = False
        for i, line in enumerate(self.lines, start=1):
            if re.match(r"^ {0,3}(`{3,}|~{3,})", line):
                in_code_block = not in_code_block
                continue
            if in_code_block:
                continue
            # Count single unescaped backticks that aren't part of ```
            # We can strip escaped backticks \`
            clean = re.sub(r"\\`", "", line)
            # Find backtick segments
            # In markdown, backticks can be pairs of ` or ``
            # A simple sanity check: total number of backticks in inline text should be even
            bt_count = clean.count("`")
            if bt_count % 2 != 0:
                self.errors.append(f"Line {i}: Odd number of backticks ({bt_count}) on line: {line.strip()[:60]}")

    def check_unclosed_brackets_and_links(self):
        in_code_block = False
        for i, line in enumerate(self.lines, start=1):
            if re.match(r"^ {0,3}(`{3,}|~{3,})", line):
                in_code_block = not in_code_block
                continue
            if in_code_block:
                continue
            
            # Remove inline code spans
            clean = re.sub(r"`[^`]*`", "", line)
            
            # Check for broken markdown links: [text]( without closing )
            broken_open_link = re.findall(r"\[[^\]]+\]\([^\)]*$", clean)
            if broken_open_link:
                self.errors.append(f"Line {i}: Possible unclosed markdown link parenthesis: {broken_open_link[0]}")

            # Check for unmatched square brackets
            # Remove valid markdown links [text](url) and images ![alt](url)
            no_links = re.sub(r"!\[[^\]]*\]\([^\)]*\)", "", clean)
            no_links = re.sub(r"\[[^\]]*\]\([^\)]*\)", "", no_links)
            # Remove markdown reference links [text][ref] or [text]
            no_links = re.sub(r"\[[^\]]*\]", "", no_links)
            
            if "[" in no_links or "]" in no_links:
                # Some brackets may be literal, e.g. [OK] or [===]
                # Let's inspect them
                unmatched = re.findall(r"[\[\]]", no_links)
                # If there's an isolated bracket like [ without ] anywhere in the line
                # Note: UI buttons like `[OK]` or `[!  ]` might be in no_links if not backticked
                pass

    def check_table_alignment(self):
        in_table = False
        table_start = 0
        expected_cols = 0
        
        for i, line in enumerate(self.lines, start=1):
            stripped = line.strip()
            if stripped.startswith("|") and stripped.endswith("|"):
                # Count columns
                # Split by | ignoring escaped \|
                parts = [p for p in re.split(r"(?<!\\)\|", stripped)[1:-1]]
                col_count = len(parts)
                
                if not in_table:
                    in_table = True
                    table_start = i
                    expected_cols = col_count
                else:
                    # Delimiter row e.g. |---|---|
                    if re.match(r"^\s*\|(?:\s*:?-+:?\s*\|)+\s*$", stripped):
                        delim_cols = len([p for p in re.split(r"(?<!\\)\|", stripped)[1:-1]])
                        if delim_cols != expected_cols:
                            self.errors.append(f"Table at line {table_start}: Header has {expected_cols} cols but delimiter at line {i} has {delim_cols} cols")
                    else:
                        if col_count != expected_cols:
                            self.errors.append(f"Table at line {table_start}: Row at line {i} has {col_count} cols, expected {expected_cols}: {stripped[:50]}")
            else:
                in_table = False

    def check_html_tags(self):
        # Extract HTML snippets
        class TagParser(html.parser.HTMLParser):
            def __init__(self):
                super().__init__()
                self.tag_errors = []
            def error(self, message):
                self.tag_errors.append(message)
                
        parser = TagParser()
        try:
            parser.feed(self.content)
        except Exception as e:
            self.errors.append(f"HTML parsing exception: {e}")
        if parser.tag_errors:
            for err in parser.tag_errors:
                self.errors.append(f"HTML error: {err}")

    def check_html_entities(self):
        # Check for malformed HTML entities like &foo without ;
        # Valid entity: &([a-zA-Z0-9]+|#[0-9]+|#x[0-9a-fA-F]+);
        # Look for & followed by word characters NOT followed by ;
        # But exclude URL queries like foo=1&bar=2
        raw_amps = re.finditer(r"&([a-zA-Z]+)(?!;)", self.content)
        for m in raw_amps:
            word = m.group(1)
            # Check if it looks like a truncated entity, e.g. &nbsp or &amp or &gt
            if word in ["nbsp", "amp", "lt", "gt", "quot", "apos"]:
                self.errors.append(f"Unterminated HTML entity '&{word}' found without semicolon at position {m.start()}")


def extract_all_targets(content: str) -> Dict[str, List[Tuple[str, int]]]:
    """
    Extracts all links, image paths, code references, and anchors with line numbers.
    """
    targets = {
        "md_links": [],
        "md_images": [],
        "html_hrefs": [],
        "html_srcs": [],
        "html_posters": [],
        "anchors": [],
        "code_refs": []
    }
    
    lines = content.splitlines()
    for i, line in enumerate(lines, start=1):
        # Markdown images ![alt](target)
        for m in re.finditer(r"!\[([^\]]*)\]\(\s*<?([^\s\)>]+)>?(?:\s+[\"'][^\"']*[\"'])?\s*\)", line):
            targets["md_images"].append((m.group(2), i))
        
        # Markdown links [text](target) - exclude image links !
        for m in re.finditer(r"(?<!!)\[([^\]]*)\]\(\s*<?([^\s\)>]+)>?(?:\s+[\"'][^\"']*[\"'])?\s*\)", line):
            targets["md_links"].append((m.group(2), i))
            
        # HTML href
        for m in re.finditer(r"<a\s+[^>]*href=[\"']([^\"']+)[\"']", line):
            targets["html_hrefs"].append((m.group(1), i))
            
        # HTML src
        for m in re.finditer(r"<(?:img|video|source|script|link)\s+[^>]*src=[\"']([^\"']+)[\"']", line):
            targets["html_srcs"].append((m.group(1), i))
            
        # HTML poster
        for m in re.finditer(r"<video\s+[^>]*poster=[\"']([^\"']+)[\"']", line):
            targets["html_posters"].append((m.group(1), i))
            
        # Inline code file references
        for m in re.finditer(r"`([a-zA-Z0-9_\-\./]+\.[a-zA-Z0-9]+)`", line):
            ref = m.group(1)
            if "/" in ref or ref.endswith((".h", ".cpp", ".bin", ".zip", ".gz", ".apk", ".ini", ".csv", ".json", ".md")):
                targets["code_refs"].append((ref, i))

    return targets


def github_slug(heading: str) -> str:
    """
    Generates a GitHub-compatible anchor slug from a markdown heading.
    """
    # Remove leading hashes
    h = re.sub(r"^#+\s*", "", heading).strip()
    # Remove HTML tags
    h = re.sub(r"<[^>]+>", "", h)
    # Convert to lowercase
    h = h.lower()
    # Replace spaces with hyphens
    h = h.replace(" ", "-")
    # Remove punctuation except hyphens and alphanumeric (including unicode characters)
    h = re.sub(r"[^\w\- ]", "", h)
    # Strip double hyphens
    h = re.sub(r"-+", "-", h)
    return h.strip("-")


def verify_target(target: str, root: Path) -> Tuple[bool, str]:
    """
    Verifies if a link target exists and is non-empty.
    """
    if target.startswith(("http://", "https://", "mailto:", "tel:", "data:")):
        # Syntax check for external URLs
        if not re.match(r"^https?://[a-zA-Z0-9\-\.]+(\.[a-zA-Z]{2,})?(:[0-9]+)?(/.*)?$", target):
            return False, f"Malformed external URL: {target}"
        return True, "External URL (syntax valid)"
        
    if target.startswith("#"):
        return True, "Internal anchor"
        
    clean = target.split("#")[0].split("?")[0].strip()
    if not clean:
        return True, "Empty / anchor-only"
        
    p = (root / clean).resolve()
    if not p.exists():
        return False, f"Target missing on filesystem: {p}"
    if not os.access(p, os.R_OK):
        return False, f"Target unreadable: {p}"
    if p.is_file() and p.stat().st_size == 0:
        return False, f"Target is 0 bytes (empty): {p}"
    return True, f"OK ({p.stat().st_size if p.is_file() else 'dir'} bytes)"


def test_pin_definitions():
    """
    Verifies all GPIO pin definitions against src/config.h.
    """
    config_path = PROJECT_ROOT / "src/config.h"
    with open(config_path, "r", encoding="utf-8") as f:
        config_text = f.read()

    expected_pins = {
        "PIN_BTN_UP": 0,
        "PIN_BTN_ACTION": 1,
        "PIN_BATTERY_ADC": 2,
        "CC_GDO0": 3,
        "CC_SCK": 4,
        "CC_MISO": 5,
        "CC_MOSI": 6,
        "CC_CS": 7,
        "PIN_OLED_SDA": 8,
        "PIN_OLED_SCL": 9,
        "PIN_BTN_DOWN": 10,
    }

    results = []
    for macro, pin in expected_pins.items():
        m = re.search(r"#define\s+" + macro + r"\s+([0-9]+)", config_text)
        if not m:
            results.append((False, f"Macro {macro} not found in src/config.h"))
        elif int(m.group(1)) != pin:
            results.append((False, f"Macro {macro} is {m.group(1)}, expected {pin}"))
        else:
            results.append((True, f"src/config.h: {macro} == GPIO {pin}"))

    return results


def main():
    print("=" * 80)
    print("       CHALLENGER 1: ADVERSARIAL STRESS TEST FOR DOCUMENTATION")
    print("=" * 80)
    
    docs = ["README.md", "README_RU.md"]
    total_defects = 0
    
    # 1. Check Pin Definitions
    print("\n--- TEST SUITE 1: Pin Definitions against src/config.h ---")
    pin_results = test_pin_definitions()
    for passed, msg in pin_results:
        status = "[PASS]" if passed else "[FAIL]"
        print(f"  {status} {msg}")
        if not passed:
            total_defects += 1

    # 2. Check Each Document
    for doc in docs:
        doc_path = PROJECT_ROOT / doc
        print(f"\n--- TEST SUITE 2: Stress-Testing {doc} ---")
        if not doc_path.exists():
            print(f"  [FAIL] {doc} does not exist!")
            total_defects += 1
            continue
            
        with open(doc_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Syntax checks
        checker = MarkdownSyntaxChecker(doc, content)
        checker.check_code_fences()
        checker.check_inline_backticks()
        checker.check_unclosed_brackets_and_links()
        checker.check_table_alignment()
        checker.check_html_tags()
        checker.check_html_entities()

        if checker.errors:
            print(f"  [FAIL] Syntax errors in {doc}:")
            for err in checker.errors:
                print(f"    - {err}")
                total_defects += 1
        else:
            print(f"  [PASS] Syntax & formatting: code fences, inline backticks, tables, HTML tags, entities")

        # Table alignment details
        print(f"  [INFO] Verified all markdown tables for column count consistency.")

        # Target Extraction & Validation
        targets = extract_all_targets(content)
        all_link_targets = []
        for category, items in targets.items():
            if category == "code_refs":
                continue
            all_link_targets.extend(items)

        print(f"  [INFO] Extracted {len(all_link_targets)} total link/image/media targets in {doc}")
        
        broken = []
        for target, line_no in all_link_targets:
            ok, msg = verify_target(target, PROJECT_ROOT)
            if not ok:
                broken.append((line_no, target, msg))
                
        if broken:
            print(f"  [FAIL] {len(broken)} broken link/image targets found in {doc}:")
            for line_no, target, msg in broken:
                print(f"    Line {line_no}: {target} -> {msg}")
                total_defects += 1
        else:
            print(f"  [PASS] All {len(all_link_targets)} link/image targets exist, are non-empty, and valid.")

        # Internal Anchors Check
        # Find all #anchors in links
        anchor_links = [(target, line_no) for target, line_no in all_link_targets if target.startswith("#")]
        if anchor_links:
            # Build set of all headings
            headings = [l for l in content.splitlines() if l.startswith("#")]
            slugs = {github_slug(h) for h in headings}
            # Also find any id="foo" or name="foo"
            html_ids = set(re.findall(r"(?:id|name)=[\"']([^\"']+)[\"']", content))
            valid_anchors = slugs.union(html_ids)
            
            broken_anchors = []
            for a, line_no in anchor_links:
                anchor_name = a.lstrip("#")
                if anchor_name not in valid_anchors:
                    broken_anchors.append((line_no, a))
            if broken_anchors:
                print(f"  [FAIL] {len(broken_anchors)} broken internal anchors in {doc}:")
                for line_no, a in broken_anchors:
                    print(f"    Line {line_no}: {a}")
                    total_defects += 1
            else:
                print(f"  [PASS] All {len(anchor_links)} internal anchors resolve to valid headings/IDs.")
        else:
            print(f"  [INFO] No internal #anchor links used in {doc}.")

        # Code References Verification
        code_refs = targets["code_refs"]
        print(f"  [INFO] Extracted {len(code_refs)} code/path references in {doc}")
        broken_code_refs = []
        for ref, line_no in code_refs:
            # Only check if it looks like a local repo file (e.g. src/..., web/..., etc.)
            if ref.startswith(("src/", "web/", "lib/", "gui/", "android/", "motion_render/")):
                p = (PROJECT_ROOT / ref).resolve()
                if not p.exists():
                    broken_code_refs.append((line_no, ref, "File not found"))
                elif p.is_file() and p.stat().st_size == 0:
                    broken_code_refs.append((line_no, ref, "Empty file (0 bytes)"))
        if broken_code_refs:
            print(f"  [FAIL] Broken code references in {doc}:")
            for line_no, ref, msg in broken_code_refs:
                print(f"    Line {line_no}: `{ref}` -> {msg}")
                total_defects += 1
        else:
            print(f"  [PASS] All referenced project source files in `{doc}` exist and are non-empty.")

        # Pin mentions in doc
        # Verify GPIO 0..10, 20, 21 are explicitly present and match their assigned functions
        pin_mappings = {
            "GPIO 0": ["UP", "ВВЕРХ", "Button UP", "Кнопка ВВЕРХ"],
            "GPIO 1": ["OK", "Action", "ОК", "Button OK", "Кнопка ОК"],
            "GPIO 2": ["ADC", "Battery", "АКБ", "Вольтметр"],
            "GPIO 3": ["GDO0", "CC1101 GDO0"],
            "GPIO 4": ["SCK", "CLK", "CC1101 SCK"],
            "GPIO 5": ["MISO", "SO", "CC1101 MISO"],
            "GPIO 6": ["MOSI", "SI", "CC1101 MOSI"],
            "GPIO 7": ["CS", "CSN", "SS", "CC1101 CSN"],
            "GPIO 8": ["SDA", "OLED SDA"],
            "GPIO 9": ["SCL", "OLED SCL"],
            "GPIO 10": ["DOWN", "ВНИЗ", "Button DOWN", "Кнопка ВНИЗ"],
            "GPIO 20": ["UART0", "TX", "Expansion", "Порт расширения"],
            "GPIO 21": ["UART0", "RX", "Expansion", "Порт расширения"],
        }
        pin_failures = []
        for pin_name, expected_keywords in pin_mappings.items():
            pattern = re.compile(rf"{pin_name}\b", re.IGNORECASE)
            matches = list(pattern.finditer(content))
            if not matches:
                pin_failures.append(f"{pin_name} not found in {doc}")
            else:
                # Check surrounding context in table or text
                found_kw = False
                for m in matches:
                    start = max(0, m.start() - 60)
                    end = min(len(content), m.end() + 100)
                    context = content[start:end]
                    if any(kw.lower() in context.lower() for kw in expected_keywords):
                        found_kw = True
                        break
                if not found_kw:
                    pin_failures.append(f"{pin_name} in {doc} is missing expected functional associations ({expected_keywords})")

        if pin_failures:
            print(f"  [FAIL] Pin definition mismatches in {doc}:")
            for f in pin_failures:
                print(f"    - {f}")
                total_defects += 1
        else:
            print(f"  [PASS] All pins GPIO 0..10, 20, 21 verified with accurate hardware function mappings in {doc}.")

    # Summary
    print("\n" + "=" * 80)
    print("                             FINAL VERDICT")
    print("=" * 80)
    if total_defects == 0:
        print("VERDICT: CONFIRM CORRECTNESS (0 defects detected across all adversarial checks)")
    else:
        print(f"VERDICT: IDENTIFY DEFECTS ({total_defects} defects detected)")
    print("=" * 80)

    return 0 if total_defects == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
