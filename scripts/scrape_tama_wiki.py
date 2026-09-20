#!/usr/bin/env python3
"""Scrape Tama Search characters and emit firmware catalog assets."""

from __future__ import annotations

import json
import re
import sys
import time
import urllib.error
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Install Pillow first: pip install pillow", file=sys.stderr)
    sys.exit(1)

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "main" / "generated"
SPRITE_BIN = OUT_DIR / "tama_sprites.bin"
CATALOG_C = OUT_DIR / "tama_catalog.c"
CACHE_DIR = Path(__file__).resolve().parent / ".cache"
SPRITE_SIZE = 120
WIKI_TITLE = "Tamagotchi_Uni/Tama_Search_characters"
WIKI_API = "https://tamagotchi.fandom.com/api.php"
WIKI_PAGE = "https://tamagotchi.fandom.com/wiki/Tamagotchi_Uni/Tama_Search_characters"
USER_AGENT = (
    "TamaSearchHelper/1.0 (personal ESP32 firmware; "
    "+https://tamagotchi.fandom.com/wiki/Tamagotchi_Uni/Tama_Search_characters)"
)

MAC_CHARACTERS = [
    ("Mametchi", 0x00, 0x00, None),
    ("Weeptchi", 0x00, 0x10, None),
    ("Hypertchi", 0x00, 0x20, None),
    ("Kuchipatchi", 0x00, 0x30, None),
    ("Shykutchi", 0x00, 0x40, None),
    ("Bigsmile", 0x00, 0x50, None),
    ("Kikitchi", 0x00, 0x60, None),
    ("Simagurutchi", 0x00, 0x70, None),
    ("Gozarutchi", 0x00, 0x80, None),
    ("Milktchi", 0x00, 0x90, None),
    ("Mimitchi", 0x00, 0xA0, None),
    ("Picochutchi", 0x00, 0xB0, None),
    ("Memetchi", 0x00, 0xC0, None),
    ("Bubbletchi", 0x00, 0xD0, None),
    ("Woopatchi", 0x00, 0xE0, None),
    ("Neliatchi", 0x00, 0xF0, None),
    ("Sebiretchi", 0x01, 0x00, None),
    ("Momotchi", 0x01, 0x10, None),
    ("Unimarutchi", 0x01, 0x20, None),
    ("Simasimatchi", 0x01, 0x30, None),
    ("Yattatchi", 0x01, 0x40, None),
    ("Nazotchi", 0x01, 0x50, None),
    ("Maidtchi", 0x01, 0x60, None),
    ("Uwasatchi", 0x01, 0x70, None),
    ("Shirimotchi", 0x01, 0x80, None),
    ("Chukatchi", 0x01, 0x90, None),
    ("Watawatatchi", 0x01, 0xA0, None),
    ("Crayontchi", 0x01, 0xB0, None),
    ("Pierrotchi", 0x01, 0xC0, None),
    ("Majokkotchi", 0x01, 0xD0, None),
    ("Hatakemotchi", 0x01, 0xE0, None),
    ("Butterflytchi", 0x01, 0xF0, None),
    ("Attendant", 0x02, 0x00, None),
    ("Maskutchi", 0x02, 0x10, None),
    ("Ichirinshatchi", 0x02, 0x20, None),
    ("Nonopotchi", 0x02, 0x30, None),
    ("Himebaratchi", 0x02, 0x40, None),
    ("Pichipitchi", 0x02, 0x50, None),
    ("Youmotchi", 0x02, 0x60, None),
    ("Fairytchi", 0x02, 0x70, None),
    ("Majoritchi", 0x02, 0x80, None),
    ("Madamtchi", 0x02, 0x90, None),
    ("Lovesolatchi", 0x02, 0xA0, None),
    ("Miraitchi", 0x02, 0xB0, None),
    ("Clulutchi", 0x02, 0xC0, None),
    ("Morijikatchi", 0x02, 0xD0, None),
    ("Guriguritchi", 0x02, 0xE0, None),
    ("Sunopotchi", 0x02, 0xF0, None),
    ("Rinkurutchi", 0x03, 0x00, None),
    ("Oyajitchi", 0x03, 0x10, None),
    ("Charatchi", 0x03, 0x20, None),
    ("Ninjanyatchi", 0x03, 0x30, None),
    ("Paintotchi", 0x03, 0x40, None),
    ("Furawatchi", 0x03, 0x50, None),
    ("Chamametchi", 0x03, 0x60, None),
    ("Shinshitchi", 0x03, 0x70, None),
    ("Businosuketchi", 0x03, 0x80, None),
    ("Marutchi", 0x03, 0x90, None),
    ("Orenetchi", 0x03, 0xA0, None),
    ("Nenetchi", 0x03, 0xB0, None),
    ("Himespetchi", 0x03, 0xC0, None),
    ("Himetchi", 0x03, 0xD0, None),
    ("Kyawatchi", 0x03, 0xE0, None),
    ("Motetchi", 0x03, 0xF0, None),
    ("Rosetchi", 0x00, 0x0F, "spring"),
    ("Yotsubatchi", 0x01, 0x0F, "spring"),
    ("Hanafuwatchi", 0x02, 0x0F, "spring"),
    ("Musiharutchi", 0x03, 0x0F, "spring"),
    ("Acchitchi", 0x00, 0x0E, "summer"),
    ("Tokonatchi", 0x01, 0x0E, "summer"),
    ("Tropicatchi", 0x02, 0x0E, "summer"),
    ("Yashiharotchi", 0x03, 0x0E, "summer"),
    ("Dekotchi", 0x00, 0x0D, "fall"),
    ("Youngdrotchi", 0x01, 0x0D, "fall"),
    ("Witchi", 0x02, 0x0D, "fall"),
    ("Pumpkitchi", 0x03, 0x0D, "fall"),
    ("Amiamitchi", 0x00, 0x0C, "winter"),
    ("Santaclautchi", 0x01, 0x0C, "winter"),
    ("Akahanatchi", 0x02, 0x0C, "winter"),
    ("Yukipatchi", 0x03, 0x0C, "winter"),
]

SSID_CHARACTERS = [
    ("Angel & Devil", "FyKHSZlwQCzTpGuDHrJclhA2Kq9vYNdP"),
    ("Makiko", "oHWqLZuDba3HjCwemPXc61et9lKpDE60"),
    ("Snowmarutchi", "TtKihQXLtUvf8Pg4pPfzgZNm3cMifPW2"),
    ("Manekimimitchi", "BS5nm6JYUGABKZKGFNWEpMM7Vag4qUeB"),
    ("MasamunePatchi", "n1ngVgVtJ6Q4l7HZ3RLbAKI7KdkByZLa"),
    ("GirlyMametchi", "S0ccc5tmCxe69la0ff9UEBVy42gDzirG"),
    ("WaiterMametchi", "XqCIzP70WUOOgmuYJyN71bn39PmOhRXS"),
    ("MomKuchipatchi", "bDwg0TAt3bFJvrBUd9Zh4Z71cJ9NbihX"),
    ("MentaiMametchi", "4lcA42klr3UlQh3iPafHggFuHwNmQDkA"),
    ("Youngmametchi", "r71676YmaL7BgMqjQwoU9SVuZGDMRDLK"),
    ("1123 Mametchi", "cK4zCzlkZRZZRhQgVev42ghANdfscPGx"),
    ("atre Memetchi", "eczNNK2nzEKFuGQuBcf8UvKymLhBFeTQ"),
    ("Pochitchi", "pQYeFibtVXgJhOtNgDOEXC5ygkjkpJKC"),
    ("Asa&Yorutchi", "gCjUrU3DwNsjpIWsh53bDAI2g72KoxL8"),
    ("Tamako hime", "1xuULYofy8K4yN6h0eCJpZ45qkKJH5cT"),
    ("20th Yattatchi", "4w4AHuUMZw5IJ6n8sGCviXPzO1RZz16g"),
    ("Factory Tama", "YsDXsevVKDBK1f5xTed6XkYqUrCpazrQ"),
    ("Milktchi", "JuqfEv8zFn8LnJdkUfkCgBRU7YfVf9nG"),
    ("Yumemitchi", "AYlvLBg6Ex8WoDdmk1AftZ5E3evPhYll"),
    ("Tamahiko oji", "QvWG15JJdsaqejxoNr0Iyc9PPtheRHFg"),
    ("M Hello Kitty", "sleVMCdb7AuJJDNBj9CJD9CPPkKdGfib"),
    ("ShisaPatchi", "Xd3jj6F7UYaV4mwMoEUR0HTf9xMLWHVm"),
]

PALETTE = [
    (255, 196, 90),
    (255, 140, 170),
    (120, 200, 255),
    (160, 220, 120),
    (200, 160, 255),
    (255, 170, 90),
    (130, 230, 210),
    (255, 120, 120),
]


def http_get(url: str, binary: bool = False):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "*/*"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = resp.read()
    return data if binary else data.decode("utf-8", errors="replace")


def fetch_wiki_html() -> str | None:
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    cached = CACHE_DIR / "wiki.html"
    try:
        params = (
            f"{WIKI_API}?action=parse&page={WIKI_TITLE}"
            "&prop=text&format=json&formatversion=2"
        )
        payload = json.loads(http_get(params))
        html = payload.get("parse", {}).get("text", "")
        if html:
            cached.write_text(html, encoding="utf-8")
            print(f"Fetched wiki HTML via API ({len(html)} bytes)")
            return html
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, json.JSONDecodeError) as exc:
        print(f"Wiki API failed: {exc}")
    try:
        html = http_get(WIKI_PAGE)
        cached.write_text(html, encoding="utf-8")
        print(f"Fetched wiki HTML via page ({len(html)} bytes)")
        return html
    except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError) as exc:
        print(f"Wiki page fetch failed: {exc}")
        if cached.exists():
            print("Using cached wiki HTML")
            return cached.read_text(encoding="utf-8")
    return None


class WikiTableParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.tables: list[list[list[dict]]] = []
        self._table: list[list[dict]] | None = None
        self._row: list[dict] | None = None
        self._cell: dict | None = None
        self._capture = False
        self._in_img = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "table" and "wikitable" in attrs.get("class", ""):
            self._table = []
        elif tag == "tr" and self._table is not None:
            self._row = []
        elif tag in ("td", "th") and self._row is not None:
            self._cell = {"text": "", "img": None}
            self._capture = True
        elif tag == "img" and self._cell is not None:
            src = attrs.get("data-src") or attrs.get("src") or ""
            src = src.split("/revision/")[0]
            if src.startswith("//"):
                src = "https:" + src
            elif src.startswith("/"):
                src = "https://tamagotchi.fandom.com" + src
            self._cell["img"] = src

    def handle_endtag(self, tag):
        if tag in ("td", "th") and self._cell is not None and self._row is not None:
            self._cell["text"] = re.sub(r"\s+", " ", self._cell["text"]).strip()
            self._row.append(self._cell)
            self._cell = None
            self._capture = False
        elif tag == "tr" and self._row is not None and self._table is not None:
            if self._row:
                self._table.append(self._row)
            self._row = None
        elif tag == "table" and self._table is not None:
            if self._table:
                self.tables.append(self._table)
            self._table = None

    def handle_data(self, data):
        if self._capture and self._cell is not None:
            self._cell["text"] += data


def norm_name(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", name.lower())


def parse_mac(text: str) -> tuple[int, int] | None:
    match = re.search(r"([0-9A-Fa-f]{2})\s*:\s*([0-9A-Fa-f]{2})", text)
    if not match:
        return None
    return int(match.group(1), 16), int(match.group(2), 16)


def wiki_sprite_map(html: str | None) -> dict[str, str]:
    if not html:
        return {}
    parser = WikiTableParser()
    parser.feed(html)
    mapping: dict[str, str] = {}
    for table in parser.tables:
        if not table:
            continue
        header = [cell["text"].lower() for cell in table[0]]
        name_idx = next((i for i, h in enumerate(header) if "name" in h), None)
        sprite_idx = next((i for i, h in enumerate(header) if "sprite" in h), None)
        if name_idx is None or sprite_idx is None:
            continue
        for row in table[1:]:
            if max(name_idx, sprite_idx) >= len(row):
                continue
            name = row[name_idx]["text"]
            img = row[sprite_idx]["img"]
            if name and img:
                mapping[norm_name(name)] = img
                mapping[norm_name(name.replace("chi", ""))] = img
    print(f"Parsed {len(mapping)} sprite URLs from wiki tables")
    return mapping


def sprite_urls(url: str) -> list[str]:
    urls = [url]
    if "/revision/" not in url:
        urls.append(url + "/revision/latest")
        urls.append(url + "/revision/latest/scale-to-width-down/120")
    return urls


def download_sprite(url: str, dest: Path) -> bool:
    if dest.exists() and dest.stat().st_size > 0:
        return True
    last_error = None
    for candidate in sprite_urls(url):
        for attempt in range(3):
            try:
                data = http_get(candidate, binary=True)
                if data:
                    dest.write_bytes(data)
                    return True
            except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError) as exc:
                last_error = exc
                time.sleep(0.4 * (attempt + 1))
    print(f"  sprite download failed ({url}): {last_error}")
    return False


def placeholder_image(name: str, index: int) -> Image.Image:
    color = PALETTE[index % len(PALETTE)]
    img = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    pad = 6
    draw.ellipse((pad, pad, SPRITE_SIZE - pad - 1, SPRITE_SIZE - pad - 1), fill=color + (255,))
    eye_y = SPRITE_SIZE // 2 - 10
    draw.ellipse((40, eye_y, 52, eye_y + 16), fill=(40, 30, 20, 255))
    draw.ellipse((68, eye_y, 80, eye_y + 16), fill=(40, 30, 20, 255))
    initials = re.sub(r"[^A-Za-z0-9]", "", name)[:2] or "?"
    try:
        font = ImageFont.truetype("arial.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    bbox = draw.textbbox((0, 0), initials, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(
        ((SPRITE_SIZE - tw) / 2, SPRITE_SIZE * 0.68),
        initials,
        fill=(40, 30, 20, 255),
        font=font,
    )
    return img


def load_sprite_image(path: Path | None, name: str, index: int) -> Image.Image:
    if path and path.exists():
        try:
            src = Image.open(path)
            src = src.convert("RGBA")
            src.thumbnail((SPRITE_SIZE, SPRITE_SIZE), Image.NEAREST)
            canvas = Image.new("RGBA", (SPRITE_SIZE, SPRITE_SIZE), (0, 0, 0, 0))
            x = (SPRITE_SIZE - src.width) // 2
            y = (SPRITE_SIZE - src.height) // 2
            canvas.paste(src, (x, y), src)
            return canvas
        except OSError as exc:
            print(f"  could not decode {path.name}: {exc}")
    return placeholder_image(name, index)


def to_rgb565a8(img: Image.Image) -> bytes:
    img = img.convert("RGBA").resize((SPRITE_SIZE, SPRITE_SIZE), Image.NEAREST)
    rgb565 = bytearray()
    alpha = bytearray()
    pixels = img.load()
    for y in range(SPRITE_SIZE):
        for x in range(SPRITE_SIZE):
            r, g, b, a = pixels[x, y]
            value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            rgb565.append(value & 0xFF)
            rgb565.append((value >> 8) & 0xFF)
            alpha.append(a)
    return bytes(rgb565) + bytes(alpha)


def c_string(value: str | None) -> str:
    if value is None:
        return "NULL"
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f"\"{escaped}\""


def build_characters() -> list[dict]:
    chars = []
    for name, hi, lo, season in MAC_CHARACTERS:
        display = f"{name} ({season.title()})" if season else name
        chars.append(
            {
                "name": display,
                "lookup": name,
                "kind": "TAMA_KIND_MAC",
                "section": "TAMA_SECTION_NORMAL",
                "mac_hi": hi,
                "mac_lo": lo,
                "ssid": None,
            }
        )
    for name, ssid in SSID_CHARACTERS:
        chars.append(
            {
                "name": name,
                "lookup": name,
                "kind": "TAMA_KIND_SSID",
                "section": "TAMA_SECTION_SPECIAL",
                "mac_hi": 0,
                "mac_lo": 0,
                "ssid": ssid,
            }
        )
    return chars


def write_catalog(chars: list[dict]) -> None:
    lines = [
        '#include "tama_catalog.h"',
        "",
        "const tama_character_t tama_characters[] = {",
    ]
    for ch in chars:
        lines.append(
            "    {"
            f" {c_string(ch['name'])}, {ch['kind']}, {ch['section']},"
            f" 0x{ch['mac_hi']:02X}, 0x{ch['mac_lo']:02X}, {c_string(ch['ssid'])} "
            "},"
        )
    lines.append("};")
    lines.append("")
    lines.append(f"const size_t tama_character_count = {len(chars)};")
    lines.append("")
    CATALOG_C.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    sprite_cache = CACHE_DIR / "sprites"
    sprite_cache.mkdir(exist_ok=True)

    chars = build_characters()
    sprites = wiki_sprite_map(fetch_wiki_html())
    blob = bytearray()
    used_wiki = 0

    for index, ch in enumerate(chars):
        key = norm_name(ch["lookup"])
        url = sprites.get(key)
        local = sprite_cache / f"{index:03d}_{key}.img"
        got = False
        if url:
            got = download_sprite(url, local)
            if got:
                used_wiki += 1
        image = load_sprite_image(local if got else None, ch["lookup"], index)
        blob.extend(to_rgb565a8(image))
        print(f"[{index:03d}] {ch['name']}: {'wiki' if got else 'placeholder'}")

    SPRITE_BIN.write_bytes(blob)
    write_catalog(chars)
    print(f"Wrote {CATALOG_C.relative_to(ROOT)} ({len(chars)} characters)")
    print(f"Wrote {SPRITE_BIN.relative_to(ROOT)} ({len(blob)} bytes, {used_wiki} wiki sprites)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
