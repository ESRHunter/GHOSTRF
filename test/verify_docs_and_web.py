#!/usr/bin/env python3
"""
GHOSTRF Documentation & Web Integrity Verification Suite
=========================================================
Automated Quality Verification Suite (Milestone 3).

Validates:
  Suite 1: Bilingual Documentation & Link Validator (README.md & README_RU.md)
  Suite 2: Release Assets & Web Asset Integrity (web/ release files, manifest.json, graphics)
  Suite 3: Web DOM, CSS, Video & Flasher Validation (HTMLParser, brutalist CSS, MP4 ftyp box)

Dependencies: 100% Python Standard Library (Python 3.8+).
Exit codes:
  0: All tests passed (100% success)
  1: Any assertion failed
"""

import argparse
import html.parser
import json
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


class Color:
    """ANSI color codes for terminal output formatting."""
    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    WHITE = "\033[37m"
    GRAY = "\033[90m"

    @classmethod
    def disable(cls):
        """Disable all ANSI formatting."""
        cls.RESET = ""
        cls.BOLD = ""
        cls.DIM = ""
        cls.RED = ""
        cls.GREEN = ""
        cls.YELLOW = ""
        cls.BLUE = ""
        cls.MAGENTA = ""
        cls.CYAN = ""
        cls.WHITE = ""
        cls.GRAY = ""


class CheckResult:
    """Represents the outcome of an individual assertion."""
    def __init__(self, name: str, passed: bool, message: str, details: Optional[List[str]] = None):
        self.name = name
        self.passed = passed
        self.message = message
        self.details = details or []


class TestSuite:
    """Represents a named collection of verification checks."""
    def __init__(self, number: int, title: str):
        self.number = number
        self.title = title
        self.checks: List[CheckResult] = []

    def add(self, name: str, passed: bool, message: str, details: Optional[List[str]] = None) -> CheckResult:
        result = CheckResult(name, passed, message, details)
        self.checks.append(result)
        return result

    @property
    def passed_count(self) -> int:
        return sum(1 for c in self.checks if c.passed)

    @property
    def failed_count(self) -> int:
        return sum(1 for c in self.checks if not c.passed)

    @property
    def total_count(self) -> int:
        return len(self.checks)


class WebHtmlParser(html.parser.HTMLParser):
    """Robust HTMLParser for validating web/index.html structure."""
    def __init__(self):
        super().__init__()
        self.has_doctype = False
        self.tags: List[str] = []
        self.attrs_by_tag: Dict[str, List[Dict[str, str]]] = {}
        self.all_hrefs: List[str] = []
        self.all_srcs: List[str] = []
        self.all_posters: List[str] = []
        self.title_text: str = ""
        self._in_title = False
        self.parse_errors: List[str] = []

    def handle_decl(self, decl: str):
        if "doctype html" in decl.lower():
            self.has_doctype = True

    def handle_starttag(self, tag: str, attrs: List[Tuple[str, Optional[str]]]):
        self.tags.append(tag)
        attr_dict = {k.lower(): (v if v is not None else "") for k, v in attrs}
        if tag not in self.attrs_by_tag:
            self.attrs_by_tag[tag] = []
        self.attrs_by_tag[tag].append(attr_dict)

        if "href" in attr_dict:
            self.all_hrefs.append(attr_dict["href"])
        if "src" in attr_dict:
            self.all_srcs.append(attr_dict["src"])
        if "poster" in attr_dict:
            self.all_posters.append(attr_dict["poster"])

        if tag.lower() == "title":
            self._in_title = True

    def handle_endtag(self, tag: str):
        if tag.lower() == "title":
            self._in_title = False

    def handle_data(self, data: str):
        if self._in_title:
            self.title_text += data


class DocAndWebVerifier:
    """Primary verification engine for GHOSTRF documentation and web assets."""

    def __init__(self, root: Path, verbose: bool = False, no_color: bool = False):
        self.root = root.resolve()
        self.verbose = verbose
        if no_color or not sys.stdout.isatty() and not os.environ.get("FORCE_COLOR"):
            Color.disable()

        self.suites: List[TestSuite] = []

    def log(self, text: str = ""):
        print(text)

    def print_banner(self):
        w = 80
        self.log(f"{Color.CYAN}{'=' * w}{Color.RESET}")
        self.log(f"{Color.BOLD}{Color.WHITE}          GHOSTRF DOCUMENTATION & WEB INTEGRITY VERIFICATION SUITE{Color.RESET}")
        self.log(f"{Color.CYAN}{'=' * w}{Color.RESET}")
        self.log(f"{Color.GRAY}Project Root: {self.root}{Color.RESET}")
        self.log(f"{Color.GRAY}Platform: {sys.platform} / Python {sys.version.split()[0]}{Color.RESET}")
        self.log(f"{Color.GRAY}Target: Acceptance Criteria R1, R2, R3 (Zero Broken Links & Assets){Color.RESET}")
        self.log(f"{Color.CYAN}{'=' * w}{Color.RESET}\n")

    # -------------------------------------------------------------------------
    # SUITE 1: Bilingual Documentation & Link Validator
    # -------------------------------------------------------------------------
    def run_suite_1(self) -> TestSuite:
        suite = TestSuite(1, "Bilingual Documentation & Link Validator")
        docs = ["README.md", "README_RU.md"]

        for doc_name in docs:
            doc_path = self.root / doc_name
            prefix = f"[{doc_name}]"

            # 1. Existence and size
            if not doc_path.exists():
                suite.add(
                    f"{prefix} File exists and size > 4000 bytes",
                    False,
                    f"File not found at {doc_path}"
                )
                continue

            size = doc_path.stat().st_size
            passed = size > 4000
            suite.add(
                f"{prefix} File exists and size > 4000 bytes",
                passed,
                f"File size: {size:,} bytes (> 4,000 threshold)" if passed else f"File size too small: {size:,} bytes"
            )

            try:
                content = doc_path.read_text(encoding="utf-8")
            except Exception as e:
                suite.add(f"{prefix} File is readable UTF-8", False, f"Failed to read file: {e}")
                continue

            lines = content.splitlines()

            # 2. Markdown Syntax: Balanced code fences
            fence_lines = [(i + 1, l) for i, l in enumerate(lines) if l.strip().startswith("```")]
            fence_count = len(fence_lines)
            fences_balanced = (fence_count % 2 == 0)
            msg = f"Code fences: {fence_count} (balanced)" if fences_balanced else f"Unbalanced code fences ({fence_count} markers)"
            suite.add(
                f"{prefix} Markdown syntax: balanced code fences",
                fences_balanced,
                msg,
                details=[f"Line {ln}: {txt.strip()}" for ln, txt in fence_lines] if self.verbose else None
            )

            # 3. Header hierarchy
            h1 = [l for l in lines if l.startswith("# ")]
            h2 = [l for l in lines if l.startswith("## ")]
            h3 = [l for l in lines if l.startswith("### ")]
            empty_headers = [
                i + 1 for i, l in enumerate(lines)
                if re.match(r"^\s*#{1,6}\s*$", l)
            ]
            headers_valid = len(h1) >= 1 and len(h2) >= 1 and len(h3) >= 1 and len(empty_headers) == 0
            header_msg = f"Headers valid: H1={len(h1)}, H2={len(h2)}, H3={len(h3)}, Empty={len(empty_headers)}"
            suite.add(
                f"{prefix} Markdown syntax: valid header hierarchy",
                headers_valid,
                header_msg
            )

            # 4. Reciprocal language switcher in top 30 lines
            top30 = "\n".join(lines[:30])
            target_counterpart = "README_RU.md" if doc_name == "README.md" else "README.md"
            has_switcher = target_counterpart in top30
            suite.add(
                f"{prefix} Reciprocal language switcher references {target_counterpart} in top 30 lines",
                has_switcher,
                f"Reference to {target_counterpart} found in lines 1-30" if has_switcher else f"No link to {target_counterpart} within top 30 lines"
            )

            # 5. Extract all links and check strictly relative (no file: or /home/)
            # Markdown links [text](target) and images ![alt](target)
            md_links = re.findall(r"(?:!\[|\[)([^\]]*)\]\(\s*<?([^\s\)>]+)>?(?:\s+[\"'][^\"']*[\"'])?\s*\)", content)
            # HTML tags
            html_hrefs = re.findall(r"<a\s+[^>]*href=[\"']([^\"']+)[\"']", content)
            html_srcs = re.findall(r"<(?:img|video|source|script|link)\s+[^>]*src=[\"']([^\"']+)[\"']", content)
            html_posters = re.findall(r"<video\s+[^>]*poster=[\"']([^\"']+)[\"']", content)

            all_targets: List[str] = [t[1] for t in md_links] + html_hrefs + html_srcs + html_posters

            # Check no absolute forbidden paths
            forbidden_targets = [
                t for t in all_targets
                if t.startswith("file:") or "file://" in t or "/home/" in t
            ]
            suite.add(
                f"{prefix} Strictly relative links (no 'file:' or '/home/')",
                len(forbidden_targets) == 0,
                f"Checked {len(all_targets)} link targets: zero forbidden absolute paths" if not forbidden_targets else f"Forbidden absolute links found: {forbidden_targets[:3]}"
            )

            # 6. Local relative link resolution
            broken_links: List[str] = []
            resolved_details: List[str] = []

            for target in all_targets:
                if target.startswith(("http://", "https://", "mailto:", "tel:", "#", "data:")):
                    continue
                clean = target.split("#")[0].split("?")[0].strip()
                if not clean:
                    continue

                resolved_path = (self.root / clean).resolve()
                if not resolved_path.exists():
                    broken_links.append(f"{target} -> missing: {resolved_path}")
                elif not os.access(resolved_path, os.R_OK):
                    broken_links.append(f"{target} -> unreadable: {resolved_path}")
                elif resolved_path.is_file() and resolved_path.stat().st_size == 0:
                    broken_links.append(f"{target} -> empty file (0 bytes): {resolved_path}")
                else:
                    sz = resolved_path.stat().st_size if resolved_path.is_file() else "<dir>"
                    resolved_details.append(f"OK: {target} ({sz} bytes)")

            suite.add(
                f"{prefix} Zero broken local links or image references",
                len(broken_links) == 0,
                f"All local targets resolved successfully ({len(resolved_details)} checked)" if not broken_links else f"{len(broken_links)} broken local links detected",
                details=broken_links if broken_links else (resolved_details if self.verbose else None)
            )

            # 7. Structural Content Requirements
            # ASCII art hero banner
            has_ascii_banner = bool(
                re.search(r"GHOSTRF", content) and
                (re.search(r"[╔═║╚]{5,}", content) or re.search(r"[=\-_]{8,}", content) or "SUB-GHZ" in content)
            )
            suite.add(
                f"{prefix} Required structure: ASCII art hero banner",
                has_ascii_banner,
                "Brutalist ASCII header banner present" if has_ascii_banner else "ASCII hero banner missing"
            )

            # Badges
            required_badges = ["PlatformIO", "ESP32", "CC1101", "Sub-GHz", "WebSerial", "v1.0.0", "GPL"]
            missing_badges = [b for b in required_badges if b.lower() not in content.lower()]
            suite.add(
                f"{prefix} Required structure: Project badge matrix",
                len(missing_badges) == 0,
                f"All required badges present ({', '.join(required_badges)})" if not missing_badges else f"Missing badges: {missing_badges}"
            )

            # Promo video reference
            has_promo_ref = "GHOSTRF_Official_Promo.mp4" in content
            suite.add(
                f"{prefix} Required structure: Promo video showcase (GHOSTRF_Official_Promo.mp4)",
                has_promo_ref,
                "Official promo video referenced" if has_promo_ref else "Promo video reference missing"
            )

            # Pinout Table (GPIO 0..10)
            missing_gpios = [
                i for i in range(11)
                if not re.search(r"GPIO\s*" + str(i) + r"\b", content, re.IGNORECASE)
            ]
            suite.add(
                f"{prefix} Required structure: Hardware pinout table (GPIO 0..10)",
                len(missing_gpios) == 0,
                "All pins GPIO 0 to GPIO 10 documented in pinout table" if not missing_gpios else f"Missing pin definitions for: {['GPIO ' + str(g) for g in missing_gpios]}"
            )

            # 9 Firmware Modes
            # In English and Russian variants
            mode_patterns = [
                (1, "Remote TX", [r"Remote\s*\(?TX\)?", r"Передатчик"]),
                (2, "Sniffer RX", [r"Sniffer\s*\(?RX\)?", r"Сканер\s*\(?RX\)?"]),
                (3, "RAW Sniffer", [r"RAW\s*Sniffer", r"RAW\s*сканер", r"KeeLoq"]),
                (4, "Freq Analyzer", [r"Freq(?:uency)?\s*Analyzer", r"Частотомер", r"Анализатор\s*частот"]),
                (5, "RSSI Spectrum", [r"RSSI\s*Spectrum", r"Спектр(?:оанализатор)?"]),
                (6, "Carrier / Jam", [r"Carrier\s*/\s*Jam", r"Генератор\s*/\s*Помехи", r"Глушилка"]),
                (7, "2.4GHz Tools", [r"2\.4\s*GHz\s*Tools", r"2\.4\s*ГГц", r"Wi-Fi", r"BLE"]),
                (8, "Battery Info", [r"Battery\s*(?:Info|Monitor)?", r"Батаре[яи]", r"Аккумулятор"]),
                (9, "Settings", [r"Settings", r"Настройки"])
            ]
            missing_modes = []
            for num, m_name, patterns in mode_patterns:
                matched = any(re.search(p, content, re.IGNORECASE) for p in patterns)
                if not matched:
                    missing_modes.append(f"Mode {num}: {m_name}")

            suite.add(
                f"{prefix} Required structure: Deep breakdown of all 9 firmware modes",
                len(missing_modes) == 0,
                "All 9 firmware modes documented with operational details" if not missing_modes else f"Missing mode descriptions: {missing_modes}"
            )

            # PlatformIO build command
            has_pio_cmd = "pio run" in content
            suite.add(
                f"{prefix} Required structure: PlatformIO build instructions ('pio run')",
                has_pio_cmd,
                "PlatformIO CLI build command documented" if has_pio_cmd else "Missing 'pio run' command in quick-start guide"
            )

        return suite

    # -------------------------------------------------------------------------
    # SUITE 2: Release Assets & Web Asset Integrity
    # -------------------------------------------------------------------------
    def run_suite_2(self) -> TestSuite:
        suite = TestSuite(2, "Release Assets & Web Asset Integrity")
        web_dir = self.root / "web"

        # 1. Release Binaries in web/
        release_binaries = [
            ("GHOSTRF_Official_Promo.mp4", 2_000_000),
            ("GHOSTRF-Studio.apk", 5_000_000),
            ("GHOSTRF-Studio-Desktop-Linux-x64.tar.gz", 30_000_000),
            ("GHOSTRF-Studio-Desktop-Windows-x64.zip", 30_000_000),
            ("firmware_merged.bin", 1_000_000),
            ("firmware.bin", 1_000_000),
            ("bootloader.bin", 10_000),
            ("partitions.bin", 2_000),
            ("boot_app0.bin", 5_000),
        ]

        for fname, threshold in release_binaries:
            file_path = web_dir / fname
            if not file_path.exists():
                suite.add(
                    f"Release Asset: web/{fname}",
                    False,
                    f"Asset not found at {file_path}"
                )
                continue

            sz = file_path.stat().st_size
            readable = os.access(file_path, os.R_OK)
            passed = readable and sz >= threshold
            suite.add(
                f"Release Asset: web/{fname}",
                passed,
                f"Size: {sz:,} bytes (Threshold: > {threshold:,} bytes)" if passed else f"Size check failed: {sz:,} bytes < {threshold:,} bytes"
            )

        # 2. Web Graphics and Icons
        graphics = [
            "logo.svg",
            "ghost_pixel_white.png",
            "ghost_pixel_green.png",
            "ghost_512.png",
            "ghostrf_wiring_diagram.png"
        ]
        for gname in graphics:
            gpath = web_dir / gname
            if not gpath.exists():
                suite.add(f"Web Graphic: web/{gname}", False, f"Graphic not found at {gpath}")
                continue
            sz = gpath.stat().st_size
            readable = os.access(gpath, os.R_OK)
            passed = readable and sz > 0
            suite.add(
                f"Web Graphic: web/{gname}",
                passed,
                f"Graphic verified ({sz:,} bytes)" if passed else "Graphic unreadable or empty"
            )

        # 3. ESP Web Tools manifest.json Verification
        manifest_path = web_dir / "manifest.json"
        if not manifest_path.exists():
            suite.add("ESP Web Tools Manifest: web/manifest.json", False, "manifest.json missing")
        else:
            try:
                manifest_data = json.loads(manifest_path.read_text(encoding="utf-8"))
                builds = manifest_data.get("builds", [])
                manifest_valid = isinstance(builds, list) and len(builds) > 0
                parts_valid = True
                parts_details = []

                if manifest_valid:
                    parts = builds[0].get("parts", [])
                    for part in parts:
                        p_file = part.get("path")
                        p_offset = part.get("offset")
                        if not p_file or not isinstance(p_offset, int):
                            parts_valid = False
                            parts_details.append(f"Invalid part definition: {part}")
                            continue

                        part_target = web_dir / p_file
                        if not part_target.exists() or part_target.stat().st_size == 0:
                            parts_valid = False
                            parts_details.append(f"Part file missing or empty: {p_file}")
                        else:
                            parts_details.append(f"Part: {p_file} @ 0x{p_offset:x} ({part_target.stat().st_size:,} bytes)")

                manifest_ok = manifest_valid and parts_valid
                suite.add(
                    "ESP Web Tools Manifest: web/manifest.json syntax and parts",
                    manifest_ok,
                    f"Valid manifest with {len(builds[0].get('parts', []))} partition offsets" if manifest_ok else "Invalid manifest structure",
                    details=parts_details
                )
            except Exception as e:
                suite.add("ESP Web Tools Manifest: web/manifest.json syntax and parts", False, f"JSON parse error: {e}")

        # 4. web/index.html Links and Asset Resolution
        index_path = web_dir / "index.html"
        if not index_path.exists():
            suite.add("Web Link Resolution: web/index.html", False, "index.html not found")
        else:
            html_content = index_path.read_text(encoding="utf-8")
            hrefs = re.findall(r"href=[\"']([^\"']+)[\"']", html_content)
            srcs = re.findall(r"src=[\"']([^\"']+)[\"']", html_content)
            manifests = re.findall(r"manifest=[\"']([^\"']+)[\"']", html_content)
            posters = re.findall(r"poster=[\"']([^\"']+)[\"']", html_content)

            all_web_links = hrefs + srcs + manifests + posters

            # Enforce strictly relative
            forbidden_web_links = [l for l in all_web_links if l.startswith("file:") or "file://" in l or "/home/" in l]
            suite.add(
                "Web Links: Strictly relative (no 'file:' or '/home/')",
                len(forbidden_web_links) == 0,
                f"Zero forbidden URLs in web/index.html ({len(all_web_links)} checked)" if not forbidden_web_links else f"Forbidden links: {forbidden_web_links}"
            )

            broken_web_assets: List[str] = []
            verified_web_assets: List[str] = []

            for l in all_web_links:
                if l.startswith(("http://", "https://", "mailto:", "tel:", "#", "data:", "javascript:")):
                    continue
                clean = l.split("#")[0].split("?")[0].strip()
                if not clean:
                    continue

                # Local targets in web/ are relative to web/
                target_path = (web_dir / clean).resolve()
                if not target_path.exists():
                    # Check if relative to repo root instead
                    root_target = (self.root / clean).resolve()
                    if root_target.exists():
                        target_path = root_target
                    else:
                        broken_web_assets.append(f"{l} -> missing at {target_path}")
                        continue

                if not os.access(target_path, os.R_OK):
                    broken_web_assets.append(f"{l} -> unreadable at {target_path}")
                elif target_path.is_file() and target_path.stat().st_size == 0 and not clean.endswith(".nojekyll"):
                    broken_web_assets.append(f"{l} -> empty asset (0 bytes)")
                else:
                    sz = target_path.stat().st_size if target_path.is_file() else "<dir>"
                    verified_web_assets.append(f"OK: {l} ({sz} bytes)")

            suite.add(
                "Web Assets: Local relative paths exist and non-empty",
                len(broken_web_assets) == 0,
                f"All {len(verified_web_assets)} local web assets resolved cleanly" if not broken_web_assets else f"{len(broken_web_assets)} missing web assets",
                details=broken_web_assets if broken_web_assets else (verified_web_assets if self.verbose else None)
            )

        return suite

    # -------------------------------------------------------------------------
    # SUITE 3: Web DOM, CSS, Video & Flasher Validation
    # -------------------------------------------------------------------------
    def run_suite_3(self) -> TestSuite:
        suite = TestSuite(3, "Web DOM, CSS, Video & Flasher Validation")
        web_dir = self.root / "web"
        index_path = web_dir / "index.html"
        style_path = web_dir / "style.css"
        video_path = web_dir / "GHOSTRF_Official_Promo.mp4"

        # 1. HTML DOM Structure & Standard HTMLParser Parsing
        if not index_path.exists():
            suite.add("HTML DOM: web/index.html existence", False, "web/index.html not found")
        else:
            try:
                html_text = index_path.read_text(encoding="utf-8")
                parser = WebHtmlParser()
                parser.feed(html_text)

                # HTMLParser cleanly parsed
                suite.add(
                    "HTML DOM: HTMLParser cleanly parses web/index.html",
                    True,
                    f"DOM parsed cleanly ({len(parser.tags)} elements processed)"
                )

                # DOCTYPE declaration
                suite.add(
                    "HTML DOM: <!DOCTYPE html> declaration",
                    parser.has_doctype,
                    "Standard HTML5 DOCTYPE present" if parser.has_doctype else "Missing <!DOCTYPE html>"
                )

                # Responsive viewport meta tag
                meta_tags = parser.attrs_by_tag.get("meta", [])
                has_viewport = any(
                    m.get("name") == "viewport" and "width=device-width" in m.get("content", "")
                    for m in meta_tags
                )
                suite.add(
                    "HTML DOM: Responsive <meta name=\"viewport\"> tag",
                    has_viewport,
                    "Responsive mobile viewport configured" if has_viewport else "Missing standard viewport meta tag"
                )

                # Title tag
                has_title = len(parser.title_text.strip()) > 0
                suite.add(
                    "HTML DOM: Non-empty <title> tag",
                    has_title,
                    f"Title: \"{parser.title_text.strip()}\"" if has_title else "Missing or empty <title> tag"
                )

                # External CSS and JS references
                links = parser.attrs_by_tag.get("link", [])
                has_style = any("style.css" in l.get("href", "") for l in links)
                suite.add(
                    "HTML DOM: Stylesheet link (<link rel=\"stylesheet\" href=\"style.css\">)",
                    has_style,
                    "style.css properly referenced" if has_style else "style.css link missing"
                )

                scripts = parser.attrs_by_tag.get("script", [])
                has_app_js = any("app.js" in s.get("src", "") for s in scripts)
                suite.add(
                    "HTML DOM: Application script (<script src=\"app.js\">)",
                    has_app_js,
                    "app.js properly referenced" if has_app_js else "app.js script tag missing"
                )

                # Video integration referencing GHOSTRF_Official_Promo.mp4
                video_tags = parser.attrs_by_tag.get("video", [])
                source_tags = parser.attrs_by_tag.get("source", [])
                has_video_promo = any("GHOSTRF_Official_Promo.mp4" in v.get("src", "") for v in video_tags) or \
                                  any("GHOSTRF_Official_Promo.mp4" in s.get("src", "") for s in source_tags)
                suite.add(
                    "HTML DOM: Video player referencing GHOSTRF_Official_Promo.mp4",
                    has_video_promo,
                    "Interactive HTML5 <video> / <source> promo player present" if has_video_promo else "No <video> element referencing GHOSTRF_Official_Promo.mp4"
                )

                # WebSerial flasher button
                flasher_buttons = parser.attrs_by_tag.get("esp-web-install-button", [])
                has_flasher = any("manifest.json" in b.get("manifest", "") for b in flasher_buttons)
                suite.add(
                    "HTML DOM: ESP Web Tools flasher (<esp-web-install-button manifest=\"manifest.json\">)",
                    has_flasher,
                    "1-Click WebSerial installer button present" if has_flasher else "esp-web-install-button missing or manifest attribute missing"
                )

                # Active download links for all 5 release artifacts
                required_downloads = [
                    "GHOSTRF-Studio.apk",
                    "GHOSTRF-Studio-Desktop-Linux-x64.tar.gz",
                    "GHOSTRF-Studio-Desktop-Windows-x64.zip",
                    "firmware_merged.bin",
                    "firmware.bin"
                ]
                missing_downloads = [
                    d for d in required_downloads
                    if not any(d == h or d in h for h in parser.all_hrefs)
                ]
                suite.add(
                    "HTML DOM: Active download links for all 5 release artifacts",
                    len(missing_downloads) == 0,
                    f"All 5 release artifacts linked for download ({', '.join(required_downloads)})" if not missing_downloads else f"Missing download links for: {missing_downloads}"
                )

            except Exception as e:
                suite.add("HTML DOM: web/index.html parsing", False, f"HTML parsing error: {e}")

        # 2. Brutalist & Responsive CSS Validation (web/style.css)
        if not style_path.exists():
            suite.add("CSS Validation: web/style.css existence", False, "web/style.css not found")
        else:
            try:
                css_text = style_path.read_text(encoding="utf-8")

                # Balanced curly braces
                open_braces = css_text.count("{")
                close_braces = css_text.count("}")
                braces_balanced = (open_braces == close_braces)
                suite.add(
                    "CSS Validation: Balanced curly braces",
                    braces_balanced,
                    f"Braces balanced: {open_braces} open, {close_braces} close" if braces_balanced else f"Unbalanced braces: {open_braces} open vs {close_braces} close"
                )

                # Brutalist aesthetic: border-radius: 0px
                has_zero_radius = bool(re.search(r"border-radius:\s*0px", css_text))
                suite.add(
                    "CSS Validation: Brutalist square borders (border-radius: 0px)",
                    has_zero_radius,
                    "Sharp pixel aesthetic enforced (border-radius: 0px)" if has_zero_radius else "Missing brutalist border-radius: 0px rule"
                )

                # Palette definition
                has_black = "#000000" in css_text or "--c-black" in css_text
                has_white = "#ffffff" in css_text.lower() or "--c-white" in css_text
                palette_ok = has_black and has_white
                suite.add(
                    "CSS Validation: Monochrome high-contrast palette (#000000 & #ffffff)",
                    palette_ok,
                    "Strict monochrome palette variables and hex definitions verified" if palette_ok else "Missing monochrome palette definitions"
                )

                # Mobile responsiveness: @media query
                has_media_query = bool(re.search(r"@media\s*\([^)]*max-width:\s*\d+px[^)]*\)", css_text))
                suite.add(
                    "CSS Validation: Mobile responsive layout (@media max-width breakpoint)",
                    has_media_query,
                    "Responsive @media breakpoints configured" if has_media_query else "Missing mobile @media queries"
                )

            except Exception as e:
                suite.add("CSS Validation: web/style.css parsing", False, f"CSS read error: {e}")

        # 3. Promo Video Container Box Validation (ISO Base Media 'ftyp')
        if not video_path.exists():
            suite.add("Video Binary: web/GHOSTRF_Official_Promo.mp4 header", False, "web/GHOSTRF_Official_Promo.mp4 not found")
        else:
            try:
                with open(video_path, "rb") as f:
                    header = f.read(64)

                if len(header) < 32:
                    suite.add("Video Binary: web/GHOSTRF_Official_Promo.mp4 header", False, "Video file too small (< 32 bytes)")
                else:
                    box_size = int.from_bytes(header[0:4], "big")
                    box_type = header[4:8]
                    major_brand = header[8:12]

                    # Extract compatible brands
                    comp_brands = [header[i:i + 4] for i in range(16, min(box_size, len(header)), 4)]
                    all_brands = [major_brand] + comp_brands

                    is_ftyp = (box_type == b"ftyp")
                    known_brands = [b"isom", b"mp41", b"mp42", b"avc1", b"iso2", b"dash"]
                    brand_valid = any(b in known_brands for b in all_brands)

                    video_ok = is_ftyp and brand_valid
                    suite.add(
                        "Video Binary: web/GHOSTRF_Official_Promo.mp4 'ftyp' container box",
                        video_ok,
                        f"Valid ISO Base Media File (ftyp={box_type.decode(errors='replace')}, brand={major_brand.decode(errors='replace')})" if video_ok else f"Invalid video container: type={box_type}"
                    )
            except Exception as e:
                suite.add("Video Binary: web/GHOSTRF_Official_Promo.mp4 header", False, f"Binary inspection error: {e}")

        return suite

    # -------------------------------------------------------------------------
    # Main Execution & Reporting
    # -------------------------------------------------------------------------
    def run_all(self) -> int:
        self.print_banner()

        self.suites = [
            self.run_suite_1(),
            self.run_suite_2(),
            self.run_suite_3()
        ]

        total_checks = 0
        total_passed = 0
        total_failed = 0

        for suite in self.suites:
            self.log(f"{Color.BOLD}{Color.CYAN}--- Suite {suite.number}: {suite.title} ---{Color.RESET}")
            for check in suite.checks:
                total_checks += 1
                if check.passed:
                    total_passed += 1
                    status = f"{Color.GREEN}[PASS]{Color.RESET}"
                    self.log(f"  {status} {Color.WHITE}{check.name}{Color.RESET}")
                    if self.verbose:
                        self.log(f"         {Color.GRAY}-> {check.message}{Color.RESET}")
                        for detail in check.details:
                            self.log(f"            {Color.DIM}• {detail}{Color.RESET}")
                else:
                    total_failed += 1
                    status = f"{Color.RED}[FAIL]{Color.RESET}"
                    self.log(f"  {status} {Color.BOLD}{Color.RED}{check.name}{Color.RESET}")
                    self.log(f"         {Color.RED}Error: {check.message}{Color.RESET}")
                    for detail in check.details:
                        self.log(f"            {Color.YELLOW}• {detail}{Color.RESET}")
            self.log()

        # Summary Table
        w = 80
        self.log(f"{Color.CYAN}{'=' * w}{Color.RESET}")
        self.log(f"{Color.BOLD}{Color.WHITE}                             VERIFICATION SUMMARY{Color.RESET}")
        self.log(f"{Color.CYAN}{'=' * w}{Color.RESET}")
        self.log(f"{'SUITE':<50} {'TOTAL':<10} {'PASSED':<10} {'FAILED':<10}")
        self.log(f"{'-' * w}")

        for s in self.suites:
            suite_label = f"Suite {s.number}: {s.title}"[:48]
            fail_color = Color.RED if s.failed_count > 0 else Color.GREEN
            self.log(f"{suite_label:<50} {s.total_count:<10} {s.passed_count:<10} {fail_color}{s.failed_count:<10}{Color.RESET}")

        self.log(f"{'-' * w}")
        fail_color = Color.RED if total_failed > 0 else Color.GREEN
        self.log(f"{'TOTAL CHECKS':<50} {total_checks:<10} {total_passed:<10} {fail_color}{total_failed:<10}{Color.RESET}")
        self.log(f"{Color.CYAN}{'=' * w}{Color.RESET}\n")

        if total_failed == 0:
            self.log(f"{Color.BOLD}{Color.GREEN}>>> VERDICT: ALL CHECKS PASSED (100% SUCCESS) <<<{Color.RESET}")
            self.log(f"{Color.GREEN}All documentation, bilingual reciprocal links, release binaries, and web assets are fully verified.{Color.RESET}\n")
            return 0
        else:
            self.log(f"{Color.BOLD}{Color.RED}>>> VERDICT: VERIFICATION FAILED ({total_failed} checks failed) <<<{Color.RESET}")
            self.log(f"{Color.RED}Please resolve the failed assertions listed above.{Color.RESET}\n")
            return 1


def main():
    parser = argparse.ArgumentParser(
        description="GHOSTRF Documentation & Web Integrity Verification Suite"
    )
    parser.add_argument(
        "--root",
        type=str,
        default=None,
        help="Path to GHOSTRF project root (defaults to parent of test/)"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output showing all verified paths and details"
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable ANSI color codes in terminal output"
    )

    args = parser.parse_args()

    if args.root:
        root_path = Path(args.root)
    else:
        # verify_docs_and_web.py resides in test/, so parent is project root
        root_path = Path(__file__).resolve().parent.parent

    verifier = DocAndWebVerifier(root=root_path, verbose=args.verbose, no_color=args.no_color)
    sys.exit(verifier.run_all())


if __name__ == "__main__":
    main()
