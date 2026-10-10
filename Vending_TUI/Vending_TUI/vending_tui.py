#!/usr/bin/env python3
"""
Smart Vegetable & Fruit Vending Machine - Serial Monitor (Terminal UI)

Reads the STM32 UART2 log (115200 bps) and shows the machine live:
  * status line "#S key=value ..." sent by firmware (App/telemetry.c) -> dashboard
  * normal log lines ([FSM], [PAY], [WARNING], [SYSTEM] ...)        -> coloured log + sales stats

Usage:
  python vending_tui.py                 auto-detect the NUCLEO ST-LINK port
  python vending_tui.py --port COM5     choose the port (Windows)
  python vending_tui.py --port /dev/cu.usbmodem1103   (macOS)
  python vending_tui.py --demo          replay a recorded firmware session (no board needed)

Keys: 1/2/3 switch tab, r reconnect, c clear log, s save log, p pause scroll,
      t show/hide #S lines, d light/dark theme, q quit. Buttons work with the mouse too.
"""
from __future__ import annotations

import argparse
import re
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path

from rich.text import Text
from textual import on
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Horizontal, Vertical
from textual.widgets import (
    Button,
    DataTable,
    Footer,
    Header,
    Label,
    ProgressBar,
    RichLog,
    Sparkline,
    Static,
    Switch,
    TabbedContent,
    TabPane,
)

try:
    import serial  # pyserial
    from serial.tools import list_ports
except ImportError:  # demo mode still works without pyserial
    serial = None
    list_ports = None

# --------------------------------------------------------------------------- constants
BAUD_DEFAULT = 115200
TEMP_LIMIT_C = 40.0          # default; the real limit comes from the PA4 knob (tlim= in #S)
HUM_LIMIT_PCT = 70           # default; the real limit comes from SETTINGS (hlim= in #S)
PAYMENT_TIMEOUT_S = 30       # PAYMENT_TIMEOUT_SECONDS in fsm.c
COIN_VALUE_THB = 10
HEARTBEAT_LOST_S = 7.0       # firmware heartbeat is 5 s
TREND_POINTS = 150           # 150 x 2 s = 5 minutes
ST_VID = 0x0483              # STMicroelectronics (ST-LINK virtual COM port)
DEMO_TRACE = Path(__file__).with_name("demo_trace.txt")

DEFAULT_ITEMS = [  # replaced by the "----- MENU -----" table the board prints at boot
    ["Fresh Salad", 25, 3],
    ["Japanese Cucumber", 15, 2],
    ["Red Apple", 20, 1],
    ["Organic Grape", 30, 0],
]

STATE_STYLE = {
    "INIT": ("INIT", "#6b7280"),
    "IDLE": ("IDLE", "#2563eb"),
    "SELECT": ("SELECT ITEM", "#0e7490"),
    "CONFIRM": ("CONFIRM", "#0e7490"),
    "CHECK_STOCK": ("CHECK STOCK", "#0e7490"),
    "PAYMENT": ("PAYMENT", "#b45309"),
    "PAYMENT_FAILED": ("PAYMENT FAILED", "#dc2626"),
    "SAFETY_CHECK": ("SAFETY CHECK", "#9333ea"),
    "PROCESSING": ("DISPENSING", "#15803d"),
    "COMPLETE": ("COMPLETE", "#15803d"),
    "FAULT": ("LOCKOUT", "#dc2626"),
    "SETTINGS": ("SETTINGS", "#7c3aed"),
}

LOG_RULES = [  # (regex, style) first match wins
    (re.compile(r"^\[WARNING\]"), "bold red"),
    (re.compile(r"Locked|ORDER CANCELLED|TIMEOUT!|OUT OF STOCK|Sensor fault|Read failed"), "red"),
    (re.compile(r"^\[SYSTEM\]"), "bold #d97706"),
    (re.compile(r"^\[PAY\]"), "green"),
    (re.compile(r"^\[SET\]"), "bold #a78bfa"),
    (re.compile(r"^\[FSM\]"), "cyan"),
    (re.compile(r"^\[(NTC|DHT11)\]"), "magenta"),
    (re.compile(r"^#S "), "grey42"),
    (re.compile(r"^>|\| Price:|^-{5,}|^=+$"), "white"),
]

RE_STATUS_KV = re.compile(r"(\w+)=(\S+)")
RE_MENU_ROW = re.compile(r"^(.+?) \| Price: (\d+) Baht \| Stock: (\d+)$")
RE_PAY_ADD = re.compile(r"^\[PAY\] \+\d+ THB -> Paid: (\d+) / (\d+) THB")
RE_PAY_DONE = re.compile(r"^\[PAY\] Payment complete! Change: (\d+) THB")
RE_WARN_TEMP = re.compile(r"Temp(?:erature)?: (-?\d+(?:\.\d+)?) C")
RE_WARN_HUM = re.compile(r"Humid: (\d+) %")


# --------------------------------------------------------------------------- data
@dataclass
class MachineStatus:
    state: str = "INIT"
    temp: float | None = None
    tlim: float = TEMP_LIMIT_C
    hlim: int = HUM_LIMIT_PCT
    sf: int = 0                  # SETTINGS: 0 = temp field, 1 = humidity field
    spt: float = TEMP_LIMIT_C    # SETTINGS: temp value being set
    sph: int = HUM_LIMIT_PCT     # SETTINGS: humidity value being set
    hum: int | None = None
    lock: bool = False
    item: int = 0
    price: int = 0
    paid: int = 0
    left: int = 0
    prog: int = 0
    cancel: bool = False
    stock: list[int] = field(default_factory=list)


@dataclass
class SessionStats:
    sales: int = 0
    revenue: int = 0
    change_given: int = 0
    failed: int = 0
    cancelled: int = 0
    lockouts: int = 0
    sensor_errors: int = 0
    lines: int = 0
    _pending_price: int = 0


def parse_status(line: str) -> MachineStatus | None:
    """Parse '#S state=... temp=... stock=a,b,c,d' into MachineStatus."""
    kv = dict(RE_STATUS_KV.findall(line))
    if "state" not in kv:
        return None
    try:
        sensor_ok = kv.get("sens", "1") == "1"
        return MachineStatus(
            state=kv["state"],
            temp=float(kv["temp"]) if sensor_ok and "temp" in kv else None,
            tlim=float(kv.get("tlim", TEMP_LIMIT_C)),
            hlim=int(kv.get("hlim", HUM_LIMIT_PCT)),
            sf=int(kv.get("sf", 0)),
            spt=float(kv.get("spt", kv.get("tlim", TEMP_LIMIT_C))),
            sph=int(kv.get("sph", kv.get("hlim", HUM_LIMIT_PCT))),
            hum=int(kv["hum"]) if sensor_ok and "hum" in kv else None,
            lock=kv.get("lock") == "1",
            item=int(kv.get("item", 0)),
            price=int(kv.get("price", 0)),
            paid=int(kv.get("paid", 0)),
            left=int(kv.get("left", 0)),
            prog=int(kv.get("prog", 0)),
            cancel=kv.get("cancel") == "1",
            stock=[int(x) for x in kv.get("stock", "").split(",") if x.isdigit()],
        )
    except ValueError:
        return None


# --------------------------------------------------------------------------- line sources
def post(app: "VendingMonitor", fn, *args) -> None:
    """Run fn(*args) on the UI thread; ignore if the app is already closing."""
    if not app.is_running or (app.source is not None and app.source.stop_event.is_set()):
        return
    try:
        app.call_from_thread(fn, *args)
    except RuntimeError:
        pass


class SerialSource(threading.Thread):
    """Reads lines from the board, reconnects automatically if the cable is unplugged."""

    def __init__(self, app: "VendingMonitor", port: str | None, baud: int) -> None:
        super().__init__(daemon=True)
        self.app, self.port, self.baud = app, port, baud
        self.stop_event = threading.Event()
        self.reconnect_event = threading.Event()

    @staticmethod
    def find_port() -> str | None:
        if list_ports is None:
            return None
        ports = list(list_ports.comports())
        for p in ports:
            if p.vid == ST_VID:
                return p.device
        for p in ports:
            text = f"{p.description} {p.manufacturer or ''}".lower()
            if "stlink" in text or "st-link" in text or "stm32" in text:
                return p.device
        usb = [p.device for p in ports if "usb" in p.device.lower() or "acm" in p.device.lower()]
        return usb[0] if len(usb) == 1 else None

    def run(self) -> None:
        if serial is None:
            post(self.app, self.app.set_link, "pyserial not installed", False)
            return
        while not self.stop_event.is_set():
            port = self.port or self.find_port()
            if port is None:
                post(self.app, self.app.set_link, "searching for ST-LINK port...", False)
                self.stop_event.wait(1.5)
                continue
            try:
                with serial.Serial(port, self.baud, timeout=0.2) as ser:
                    self.reconnect_event.clear()
                    post(self.app, self.app.set_link, f"{port} @ {self.baud}", True)
                    buf = b""
                    while not self.stop_event.is_set() and not self.reconnect_event.is_set():
                        chunk = ser.read(256)
                        if not chunk:
                            continue
                        buf += chunk
                        while b"\n" in buf:
                            raw, buf = buf.split(b"\n", 1)
                            text = raw.decode("utf-8", errors="replace").rstrip("\r").replace("\ufffd", "").strip("\x00")
                            post(self.app, self.app.on_rx_line, text)
            except (OSError, serial.SerialException) as exc:
                post(self.app, self.app.set_link, f"{port}: {exc.__class__.__name__}, retrying", False)
                self.stop_event.wait(1.5)


class ReplaySource(threading.Thread):
    """Replays demo_trace.txt ('<ms>\\t<line>') recorded from the real firmware logic."""

    def __init__(self, app: "VendingMonitor", path: Path, speed: float) -> None:
        super().__init__(daemon=True)
        self.app, self.path, self.speed = app, path, max(speed, 0.1)
        self.stop_event = threading.Event()
        self.reconnect_event = threading.Event()

    def run(self) -> None:
        try:
            rows = []
            for raw in self.path.read_text(encoding="utf-8").splitlines():
                ms, _, text = raw.partition("\t")
                if ms.isdigit():
                    rows.append((int(ms), text))
        except OSError as exc:
            post(self.app, self.app.set_link, f"demo trace missing: {exc}", False)
            return
        while not self.stop_event.is_set():
            post(self.app, self.app.set_link, f"DEMO replay x{self.speed:g}", True)
            start = time.monotonic()
            for ms, text in rows:
                delay = ms / 1000.0 / self.speed - (time.monotonic() - start)
                if delay > 0 and self.stop_event.wait(delay):
                    return
                post(self.app, self.app.on_rx_line, text)
            self.stop_event.wait(3.0 / self.speed)


# --------------------------------------------------------------------------- widgets
def bar(value: float, limit: float, width: int = 14) -> Text:
    """Horizontal bar scaled so the limit sits at 80 % of the width."""
    scale = limit / 0.8
    filled = max(0, min(width, round(value / scale * width)))
    mark = round(0.8 * width)
    style = "green" if value <= limit * 0.9 else ("#d97706" if value <= limit else "bold red")
    out = Text()
    for i in range(width):
        if i == mark:
            out.append("│", style="white")
        out.append("█" if i < filled else "░", style=style if i < filled else "grey30")
    return out


class OledMirror(Static):
    """21 x 8 character view that follows the firmware screen logic (display.c)."""

    WIDTH = 21

    def show(self, st: MachineStatus, items: list[list]) -> None:
        name, price, stock = items[st.item] if st.item < len(items) else ("?", 0, 0)
        w = self.WIDTH
        rows: list[str]
        if st.state == "SETTINGS":
            t_sel, h_sel = ("> ", "  ") if st.sf == 0 else ("  ", "> ")
            title = "SETTINGS  !LOCKED!" if st.lock else "== SETTINGS =="
            rows = [title.center(w) if not st.lock else title, "─" * w,
                    f"{t_sel}Temp : {st.spt:.1f}C ({st.tlim:.1f})", f"{h_sel}Humid: {st.sph}%  ({st.hlim})", "",
                    "Turn knob, UP/DN sel", "─" * w, "[OK]Save [BACK]Cancel"]
            out = Text()
            for i, r in enumerate(rows):
                out.append(r[:w].ljust(w), style="bold #c4b5fd")
                if i < 7:
                    out.append("\n")
            self.update(out)
            return
        if st.lock:
            temp_bad = st.temp is not None and st.temp > st.tlim
            hum_bad = st.hum is not None and st.hum > st.hlim
            rows = ["!! SYSTEM LOCKOUT !!", "─" * w]
            if temp_bad and hum_bad:
                rows += [f"Temp: {st.temp:.1f}C [OVER]", f"Hum:  {st.hum}% [OVER]", "BOTH EXCEEDED!", ""]
            elif temp_bad:
                rows += ["TEMP OVERHEAT!", f"Current: {st.temp:.1f}C", f"Limit: {st.tlim:.1f}C", ""]
            elif hum_bad:
                rows += ["HUMIDITY OVER!", f"Current: {st.hum}%", f"Limit: {st.hlim}%", ""]
            else:
                rows += ["Checking...", "", "", ""]
            idle_msg = "Cooling Down..." if temp_bad and not hum_bad else "Waiting Normal..."
            rows += ["─" * w, "ORDER CANCELLED!" if st.cancel else idle_msg]
        elif st.state in ("PROCESSING", "COMPLETE"):
            fill = round(st.prog / 100 * 15)
            rows = ["-- DISPENSING --", "─" * w, f"Item: {name}", "",
                    "[" + "#" * fill + "." * (15 - fill) + "]", f"{st.prog:>10}%", "",
                    "Done! Enjoy your pick!" if st.state == "COMPLETE" else f"Processing... ({st.left}s)"]
        elif st.state in ("IDLE", "INIT", "FAULT"):
            t = f"{st.temp:.1f}C" if st.temp is not None else "--.-C"
            h = f"{st.hum}%" if st.hum is not None else "--%"
            rows = ["== SMART VENDING ==", "─" * w, "", "[ PRESS OK / UP ]".center(w),
                    "To Select Drink".center(w), f"Limit:{st.tlim:.1f}C / {st.hlim}%".center(w), "─" * w,
                    f"Temp:{t:<6} Hum:{h}"]
        elif st.state in ("CONFIRM", "CHECK_STOCK", "SAFETY_CHECK"):
            label = f">> {name} <<" if len(name) <= 15 else (f"> {name} <" if len(name) <= 17 else name)
            rows = ["CONFIRM ORDER", "─" * w, "", label.center(w), "",
                    f"   Price: {price} THB", "─" * w, "[OK]Yes  [BACK]Cancel"]
        elif st.state == "PAYMENT":
            need = max(st.price - st.paid, 0)
            rows = ["PAYMENT (10THB/BLOCK)", "─" * w, name, f"Price: {st.price} THB",
                    f"Paid:  {st.paid} THB".ljust(13) + f"NEED {need}".rjust(8), "", "─" * w,
                    "Cover Sensor".ljust(14) + f"Time:{st.left}s".rjust(7)]
        elif st.state == "PAYMENT_FAILED":
            rows = ["!! PAYMENT TIMEOUT !!", "─" * w, "", "PAYMENT FAILED".center(w),
                    "Returning to Menu...", "", "─" * w, "[FAIL] Money Returned"]
        else:  # SELECT
            body = ["  *OUT OF STOCK*", ""] if stock == 0 else [f"  Price: {price} THB", f"  Stock: {stock} pcs"]
            rows = [f"VEGGIE & FRUIT [{st.item + 1}/{len(items)}]", "─" * w, f"> {name}", ""] + body + \
                   ["─" * w, "UP/DN Move  OK Select"]
        rows = (rows + [""] * 8)[:8]
        style = "bold red" if st.lock else "bold #9fe8ff"
        out = Text()
        for i, r in enumerate(rows):
            out.append(r[:w].ljust(w), style=style)
            if i < 7:
                out.append("\n")
        self.update(out)


# --------------------------------------------------------------------------- app
class VendingMonitor(App):
    TITLE = "Smart Vending Monitor"
    SUB_TITLE = "STM32F411 · UART2 115200"
    CSS = """
    Screen { background: $background; }
    #toolbar { height: 3; padding: 0 1; }
    #toolbar Button { margin-right: 1; min-width: 12; }
    #link { width: 1fr; content-align: right middle; height: 3; padding-right: 1; }
    .row { height: auto; }
    .panel { border: round $primary; border-title-color: $accent; border-title-style: bold;
             padding: 0 1; width: 1fr; height: auto; }
    #state_panel { width: 30; }
    #oled_panel { width: 27; }
    #oled { background: #04121f; padding: 0 0; width: 21; }
    #state_big { text-style: bold; content-align: center middle; height: 3; }
    #env_panel Sparkline { height: 2; margin-bottom: 1; }
    #order_panel ProgressBar { width: 100%; }
    #order_panel Bar { width: 1fr; }
    #stock_table { height: 7; }
    #mini_log { height: 1fr; min-height: 6; border: round $secondary; border-title-color: $accent; }
    #full_log { height: 1fr; border: round $secondary; }
    #log_tools { height: 3; padding: 0 1; }
    #log_tools Label { padding: 1 1 0 2; }
    .trend { height: auto; border: round $primary; border-title-color: $accent; padding: 0 1; margin-bottom: 1; }
    .trend Sparkline { height: 6; }
    """

    BINDINGS = [
        Binding("1", "tab('dash')", "Dashboard"),
        Binding("2", "tab('log')", "UART Log"),
        Binding("3", "tab('trend')", "Trends"),
        Binding("r", "reconnect", "Reconnect"),
        Binding("c", "clear_log", "Clear"),
        Binding("s", "save_log", "Save log"),
        Binding("p", "pause", "Pause"),
        Binding("t", "toggle_status_lines", "#S lines"),
        Binding("d", "toggle_theme", "Theme"),
        Binding("q", "quit", "Quit"),
    ]

    def __init__(self, port: str | None, baud: int, demo: bool, speed: float) -> None:
        super().__init__()
        self.port, self.baud, self.demo, self.speed = port, baud, demo, speed
        self.status = MachineStatus(stock=[r[2] for r in DEFAULT_ITEMS])
        self.items = [list(r) for r in DEFAULT_ITEMS]
        self.stats = SessionStats()
        self.history: list[str] = []
        self.temp_hist: deque[float] = deque(maxlen=TREND_POINTS)
        self.hum_hist: deque[float] = deque(maxlen=TREND_POINTS)
        self.last_status_at = 0.0
        self.last_trend_at = 0.0
        self.connected_at = 0.0
        self.link_ok = False
        self.link_text = "starting..."
        self.paused = False
        self.show_status_lines = False
        self._menu_rows: list[list] | None = None
        self.source: SerialSource | ReplaySource | None = None

    # ----- layout
    def compose(self) -> ComposeResult:
        yield Header(show_clock=True)
        with Horizontal(id="toolbar"):
            yield Button("Reconnect", id="btn_reconnect", variant="primary")
            yield Button("Clear log", id="btn_clear")
            yield Button("Save log", id="btn_save", variant="success")
            yield Button("Pause", id="btn_pause", variant="warning")
            yield Static("", id="link")
        with TabbedContent(initial="dash"):
            with TabPane("Dashboard", id="dash"):
                with Horizontal(classes="row"):
                    with Vertical(id="state_panel", classes="panel"):
                        yield Static("", id="state_big")
                        yield Static("", id="state_info")
                    with Vertical(id="env_panel", classes="panel"):
                        yield Static("", id="env_temp")
                        yield Sparkline([], id="spark_temp", summary_function=max)
                        yield Static("", id="env_hum")
                        yield Sparkline([], id="spark_hum", summary_function=max)
                    with Vertical(id="order_panel", classes="panel"):
                        yield Static("", id="order_info")
                        yield Label("Payment time left", id="pay_label")
                        yield ProgressBar(total=PAYMENT_TIMEOUT_S, show_eta=False, show_percentage=False, id="pay_bar")
                        yield Label("Dispensing", id="disp_label")
                        yield ProgressBar(total=100, show_eta=False, show_percentage=False, id="disp_bar")
                with Horizontal(classes="row"):
                    with Vertical(id="oled_panel", classes="panel"):
                        yield OledMirror("", id="oled")
                    with Vertical(id="stock_panel", classes="panel"):
                        yield DataTable(id="stock_table", cursor_type="row", zebra_stripes=True)
                    with Vertical(id="stats_panel", classes="panel"):
                        yield Static("", id="stats")
                yield RichLog(id="mini_log", wrap=True, max_lines=400)
            with TabPane("UART Log", id="log"):
                with Horizontal(id="log_tools"):
                    yield Label("Show #S status lines")
                    yield Switch(value=False, id="sw_status")
                yield RichLog(id="full_log", wrap=True, max_lines=5000)
            with TabPane("Trends", id="trend"):
                with Vertical(classes="trend", id="trend_temp"):
                    yield Static("", id="trend_temp_txt")
                    yield Sparkline([], id="trend_temp_spark", summary_function=max)
                with Vertical(classes="trend", id="trend_hum"):
                    yield Static("", id="trend_hum_txt")
                    yield Sparkline([], id="trend_hum_spark", summary_function=max)
        yield Footer()

    def on_mount(self) -> None:
        self.query_one("#state_panel").border_title = "Machine"
        self.query_one("#env_panel").border_title = "Environment"
        self.query_one("#order_panel").border_title = "Current order"
        self.query_one("#oled_panel").border_title = "OLED mirror"
        self.query_one("#stock_panel").border_title = "Stock"
        self.query_one("#stats_panel").border_title = "Session"
        self.query_one("#mini_log").border_title = "Recent events"
        self.query_one("#full_log").border_title = "UART2 raw log"
        self.query_one("#trend_temp").border_title = "Temperature (NTC, last 5 min)"
        self.query_one("#trend_hum").border_title = "Humidity (DHT11, last 5 min)"
        table = self.query_one("#stock_table", DataTable)
        table.add_columns("#", "Item", "Price", "Stock")
        self.refresh_all()
        self.set_interval(1.0, self.tick_1s)
        self.start_source()

    # ----- sources
    def start_source(self) -> None:
        if self.source is not None:
            self.source.stop_event.set()
        if self.demo:
            self.source = ReplaySource(self, DEMO_TRACE, self.speed)
        else:
            self.source = SerialSource(self, self.port, self.baud)
        self.source.start()

    def set_link(self, text: str, ok: bool) -> None:
        if ok and not self.link_ok:
            self.connected_at = time.monotonic()
            self.write_log(f"[INFO] connected: {text}", "bold green")
        elif not ok and self.link_ok:
            self.write_log(f"[INFO] disconnected: {text}", "bold red")
        self.link_ok, self.link_text = ok, text
        self.refresh_link()

    # ----- incoming data
    def on_rx_line(self, line: str) -> None:
        self.stats.lines += 1
        if line.startswith("#S "):
            st = parse_status(line)
            if st is not None:
                self.apply_status(st)
            if self.show_status_lines:
                self.write_log(line, "grey42", mini=False)
            return
        if not line.strip():
            return
        self.parse_event(line)
        self.write_log(line, self.style_for(line))

    def apply_status(self, st: MachineStatus) -> None:
        prev = self.status
        if not st.stock:
            st.stock = prev.stock
        for i, n in enumerate(st.stock[: len(self.items)]):
            self.items[i][2] = n
        now = time.monotonic()
        moved = st.temp != prev.temp or st.hum != prev.hum
        if st.temp is not None and st.hum is not None and (moved or now - self.last_trend_at >= 2.0 / max(self.speed, 1.0)):
            self.temp_hist.append(st.temp)
            self.hum_hist.append(float(st.hum))
            self.last_trend_at = now
        self.status = st
        self.last_status_at = time.monotonic()
        self.refresh_all()

    def parse_event(self, line: str) -> None:
        s = self.stats
        if line.startswith("----------- MENU"):
            self._menu_rows = []
            return
        if self._menu_rows is not None:
            m = RE_MENU_ROW.match(line)
            if m:
                self._menu_rows.append([m.group(1), int(m.group(2)), int(m.group(3))])
                return
            if line.startswith("-----") and self._menu_rows:
                self.items = self._menu_rows
                self.status.stock = [r[2] for r in self.items]
                self._menu_rows = None
                self.refresh_all()
                return
        m = RE_PAY_ADD.match(line)
        if m:
            s._pending_price = int(m.group(2))
            return
        m = RE_PAY_DONE.match(line)
        if m:
            s.sales += 1
            s.revenue += s._pending_price
            s.change_given += int(m.group(1))
        elif line.startswith("[PAY] TIMEOUT"):
            s.failed += 1
        elif "ORDER CANCELLED" in line:
            s.cancelled += 1
        elif line.startswith("[SYSTEM] Environment Abnormal"):
            s.lockouts += 1
        elif line.startswith("[NTC]") or line.startswith("[DHT11]"):
            s.sensor_errors += 1
        elif line.startswith("[WARNING]") and self.last_status_at == 0.0:
            # firmware without telemetry: still show the values from the warning text
            mt, mh = RE_WARN_TEMP.search(line), RE_WARN_HUM.search(line)
            if mt:
                self.status.temp = float(mt.group(1))
            if mh:
                self.status.hum = int(mh.group(1))
            self.status.lock = True
        self.refresh_stats()

    @staticmethod
    def style_for(line: str) -> str:
        for rx, style in LOG_RULES:
            if rx.search(line):
                return style
        return "white"

    def write_log(self, line: str, style: str, mini: bool = True) -> None:
        stamp = datetime.now().strftime("%H:%M:%S")
        self.history.append(f"{stamp}  {line}")
        txt = Text.assemble((f"{stamp} ", "grey50"), (line, style))
        self.query_one("#full_log", RichLog).write(txt)
        noise = line.startswith(("#S ", ">", "---", "===")) or RE_MENU_ROW.match(line) or not line.startswith(("[", " "))
        if mini and not noise:
            self.query_one("#mini_log", RichLog).write(txt)

    # ----- rendering
    def refresh_all(self) -> None:
        self.refresh_state()
        self.refresh_env()
        self.refresh_order()
        self.refresh_stock()
        self.refresh_stats()
        self.refresh_trends()
        self.query_one("#oled", OledMirror).show(self.status, self.items)

    def refresh_link(self) -> None:
        dot = "[bold green]●[/]" if self.link_ok else "[bold red]●[/]"
        self.query_one("#link", Static).update(f"{dot} {self.link_text}")
        self.sub_title = self.link_text

    def refresh_state(self) -> None:
        st = self.status
        label, color = STATE_STYLE.get(st.state, (st.state, "white"))
        if st.lock and st.state != "SETTINGS":
            label, color = "LOCKOUT", "#dc2626"
        big = self.query_one("#state_big", Static)
        big.update(Text(f" {label} ", style=f"bold white on {color}"))
        age = time.monotonic() - self.last_status_at if self.last_status_at else None
        if age is None:
            alive = "[#d97706]waiting #S[/]"
        elif age < HEARTBEAT_LOST_S:
            alive = f"[green]live[/] · {age:.0f}s"
        else:
            alive = f"[red]lost · {age:.0f}s[/]"
        lock = "[bold red]LOCKED[/]" if st.lock else "[green]normal[/]"
        relay = "[bold #d97706]ON[/]" if st.lock else "off"
        self.query_one("#state_info", Static).update(
            f"FSM state : [b]{st.state}[/]\nSafety    : {lock}\nLimits    : [b #a78bfa]{st.tlim:.1f} °C / {st.hlim} %[/]\n"
            f"Fan (PC2) : {relay}\nTelemetry : {alive}")

    def refresh_env(self) -> None:
        st = self.status
        t_txt = Text("Temp  ")
        if st.temp is None:
            t_txt.append("--.- °C", style="grey50")
        else:
            t_txt.append(f"{st.temp:5.1f} °C ", style="bold red" if st.temp > st.tlim else "bold green")
            t_txt.append_text(bar(st.temp, st.tlim))
            t_txt.append(f" limit {st.tlim:.1f}", style="#a78bfa")
        h_txt = Text("Humid ")
        if st.hum is None:
            h_txt.append("-- %   ", style="grey50")
        else:
            h_txt.append(f"{st.hum:5d} %  ", style="bold red" if st.hum > st.hlim else "bold green")
            h_txt.append_text(bar(st.hum, st.hlim))
            h_txt.append(f" limit {st.hlim}", style="#a78bfa")
        self.query_one("#env_temp", Static).update(t_txt)
        self.query_one("#env_hum", Static).update(h_txt)
        self.query_one("#spark_temp", Sparkline).data = list(self.temp_hist)[-40:]
        self.query_one("#spark_hum", Sparkline).data = list(self.hum_hist)[-40:]

    def refresh_order(self) -> None:
        st = self.status
        name = self.items[st.item][0] if st.item < len(self.items) else "?"
        in_order = st.state in ("CONFIRM", "CHECK_STOCK", "PAYMENT", "SAFETY_CHECK", "PROCESSING", "COMPLETE")
        if st.state == "SETTINGS":
            field = "Temperature" if st.sf == 0 else "Humidity"
            info = (f"[b #a78bfa]SETTINGS[/] (hold BACK on home screen)\nEditing: [b]{field}[/]\n"
                    f"Temp  {st.spt:.1f} °C  (now {st.tlim:.1f})\nHumid {st.sph} %    (now {st.hlim})")
        elif st.lock and st.cancel:
            info = f"[bold red]ORDER CANCELLED[/]\nItem  : {name}\nRefund: {st.paid} THB\nStock not deducted"
        elif in_order or st.state == "PAYMENT_FAILED":
            need = max(st.price - st.paid, 0)
            coins = -(-need // COIN_VALUE_THB)
            info = (f"Item  : [b]{name}[/]\nPrice : {st.price} THB\nPaid  : [green]{st.paid}[/] THB"
                    f"   Need: [#d97706]{need}[/] THB ({coins} block{'s' if coins != 1 else ''})")
        else:
            info = "[grey50]No order in progress[/]\n\n\n"
        self.query_one("#order_info", Static).update(info)
        paying = st.state == "PAYMENT" and not st.lock
        left = st.left if paying else 0
        self.query_one("#pay_bar", ProgressBar).update(progress=left)
        self.query_one("#pay_label", Label).update(
            f"Payment time left: [b {'red' if left <= 10 else '#d97706'}]{left} s[/]" if paying else "Payment time left: -")
        dispensing = st.state in ("PROCESSING", "COMPLETE") and not st.lock
        disp = st.prog if dispensing else 0
        self.query_one("#disp_bar", ProgressBar).update(progress=disp)
        self.query_one("#disp_label", Label).update(f"Dispensing: [b green]{disp}%[/]" if dispensing else "Dispensing: -")

    def refresh_stock(self) -> None:
        table = self.query_one("#stock_table", DataTable)
        table.clear()
        for i, (name, price, stock) in enumerate(self.items):
            stock_txt = Text("SOLD OUT", style="bold red") if stock == 0 else Text(str(stock), style="green")
            table.add_row(str(i + 1), name, f"{price} THB", stock_txt)
        active = self.status.state in ("SELECT", "CONFIRM", "CHECK_STOCK", "PAYMENT", "PROCESSING", "COMPLETE")
        table.show_cursor = active and not self.status.lock
        if table.show_cursor:
            table.move_cursor(row=min(self.status.item, len(self.items) - 1))

    def refresh_stats(self) -> None:
        s = self.stats
        up = time.monotonic() - self.connected_at if self.link_ok else 0
        self.query_one("#stats", Static).update(
            f"Sales      : [b green]{s.sales}[/]   Revenue: [b green]{s.revenue}[/] THB\n"
            f"Change out : {s.change_given} THB\n"
            f"Pay failed : [#d97706]{s.failed}[/]   Cancelled: [red]{s.cancelled}[/]\n"
            f"Lockouts   : [red]{s.lockouts}[/]   Sensor err: {s.sensor_errors}\n"
            f"Lines RX   : {s.lines}   Uptime: {int(up // 60)}m{int(up % 60):02d}s")

    def refresh_trends(self) -> None:
        def summary(vals: deque, unit: str) -> str:
            if not vals:
                return "[grey50]no data yet[/]"
            return f"now {vals[-1]:.1f}{unit}   min {min(vals):.1f}{unit}   max {max(vals):.1f}{unit}   points {len(vals)}"
        self.query_one("#trend_temp_txt", Static).update(summary(self.temp_hist, " °C") + f"   limit {self.status.tlim:.1f} °C")
        self.query_one("#trend_hum_txt", Static).update(summary(self.hum_hist, " %") + f"   limit {self.status.hlim} %")
        self.query_one("#trend_temp_spark", Sparkline).data = list(self.temp_hist)
        self.query_one("#trend_hum_spark", Sparkline).data = list(self.hum_hist)

    def tick_1s(self) -> None:
        self.refresh_state()
        self.refresh_stats()

    # ----- actions
    def action_tab(self, tab: str) -> None:
        self.query_one(TabbedContent).active = tab

    def action_reconnect(self) -> None:
        self.write_log("[INFO] reconnect requested", "bold #d97706")
        if isinstance(self.source, SerialSource):
            self.source.reconnect_event.set()
        else:
            self.start_source()

    def action_clear_log(self) -> None:
        self.query_one("#full_log", RichLog).clear()
        self.query_one("#mini_log", RichLog).clear()
        self.history.clear()

    def action_save_log(self) -> None:
        path = Path.cwd() / f"vending_log_{datetime.now():%Y%m%d_%H%M%S}.txt"
        path.write_text("\n".join(self.history) + "\n", encoding="utf-8")
        self.notify(f"Saved {len(self.history)} lines to {path.name}", title="Log saved")

    def action_pause(self) -> None:
        self.paused = not self.paused
        for log in self.query(RichLog):
            log.auto_scroll = not self.paused
        self.query_one("#btn_pause", Button).label = "Resume" if self.paused else "Pause"
        self.notify("Auto-scroll paused" if self.paused else "Auto-scroll resumed")

    def action_toggle_status_lines(self) -> None:
        sw = self.query_one("#sw_status", Switch)
        sw.value = not sw.value

    def action_toggle_theme(self) -> None:
        self.theme = "textual-light" if self.theme == "textual-dark" else "textual-dark"

    @on(Switch.Changed, "#sw_status")
    def _status_switch(self, event: Switch.Changed) -> None:
        self.show_status_lines = event.value

    @on(Button.Pressed)
    def _buttons(self, event: Button.Pressed) -> None:
        actions = {"btn_reconnect": self.action_reconnect, "btn_clear": self.action_clear_log,
                   "btn_save": self.action_save_log, "btn_pause": self.action_pause}
        handler = actions.get(event.button.id or "")
        if handler:
            handler()

    def on_unmount(self) -> None:
        if self.source is not None:
            self.source.stop_event.set()


def main() -> None:
    ap = argparse.ArgumentParser(description="Serial monitor TUI for the Smart Vending Machine (STM32)")
    ap.add_argument("--port", help="serial port, e.g. COM5 or /dev/cu.usbmodem1103 (default: auto-detect ST-LINK)")
    ap.add_argument("--baud", type=int, default=BAUD_DEFAULT, help="baud rate (default 115200)")
    ap.add_argument("--demo", action="store_true", help="replay demo_trace.txt instead of opening a port")
    ap.add_argument("--speed", type=float, default=1.0, help="demo replay speed, e.g. 2 = twice as fast")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()
    if args.list:
        if list_ports is None:
            print("pyserial is not installed: pip install pyserial")
            return
        for p in list_ports.comports():
            tag = "  <- ST-LINK" if p.vid == ST_VID else ""
            print(f"{p.device:24} {p.description}{tag}")
        return
    VendingMonitor(args.port, args.baud, args.demo, args.speed).run()


if __name__ == "__main__":
    main()
