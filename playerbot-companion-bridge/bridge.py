#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import socket
import sys
from pathlib import Path
from typing import Dict, List

ROOT = Path(__file__).resolve().parent
CONFIG_PATH = ROOT / "config.json"
PROFILE_PATH = ROOT / "PROFILE.md"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORTS = (8765, 8888)
TIMEOUT = 2.5

CLASS_NAMES = {
    1: "Warrior",
    2: "Paladin",
    3: "Hunter",
    4: "Rogue",
    5: "Priest",
    7: "Shaman",
    8: "Mage",
    9: "Warlock",
    11: "Druid",
}


def one_line(value: str) -> str:
    return value.replace("\r", " ").replace("\n", " ").strip()


def send_request(host: str, port: int, command: str, selector: str = "0") -> str:
    command = one_line(command)
    selector = one_line(str(selector))
    wire = f"{command},{selector}\n".encode("utf-8")

    with socket.create_connection((host, port), timeout=TIMEOUT) as sock:
        sock.settimeout(TIMEOUT)
        sock.sendall(wire)
        data = bytearray()
        while b"\n" not in data:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data.extend(chunk)

    if not data:
        raise RuntimeError("Bridge closed the connection without a response.")

    return bytes(data).split(b"\n", 1)[0].decode("utf-8", errors="replace")


def load_config() -> dict:
    if not CONFIG_PATH.exists():
        raise RuntimeError("Not configured. Run: python bridge.py setup")
    return json.loads(CONFIG_PATH.read_text(encoding="utf-8"))


def parse_bots(raw: str) -> List[dict]:
    bots: List[dict] = []
    if not raw.strip():
        return bots

    for entry in raw.split(";"):
        parts = entry.split("|")
        if len(parts) < 5:
            continue
        guid, name, level, class_id, master = parts[:5]
        try:
            class_num = int(class_id)
        except ValueError:
            class_num = 0
        bots.append(
            {
                "guid": guid,
                "name": name,
                "level": level,
                "class_id": class_num,
                "class": CLASS_NAMES.get(class_num, f"Class {class_num}"),
                "master": master,
            }
        )
    return bots


def parse_snapshot(raw: str) -> Dict[str, str]:
    result: Dict[str, str] = {}
    for field in raw.split("\t"):
        if "=" not in field:
            continue
        key, value = field.split("=", 1)
        result[key] = value
    return result


def find_bridge() -> tuple[str, int]:
    host = DEFAULT_HOST
    for port in DEFAULT_PORTS:
        try:
            version = send_request(host, port, "bridge-version", "0")
            if version.startswith("playerbot-companion-bridge/"):
                return host, port
        except Exception:
            pass

    raw = input(f"Bridge port [{DEFAULT_PORTS[0]}]: ").strip()
    port = int(raw or DEFAULT_PORTS[0])
    version = send_request(host, port, "bridge-version", "0")
    if not version.startswith("playerbot-companion-bridge/"):
        raise RuntimeError(f"Unexpected bridge response: {version}")
    return host, port


def write_profile(name: str) -> None:
    if PROFILE_PATH.exists():
        text = PROFILE_PATH.read_text(encoding="utf-8")
        text = text.replace(
            "Name: (set during setup)",
            f"Name: {name}",
        )
        PROFILE_PATH.write_text(text, encoding="utf-8")


def setup() -> None:
    print("Playerbot Companion Bridge setup")
    print("--------------------------------")
    try:
        host, port = find_bridge()
    except Exception as exc:
        print()
        print(f"Could not reach the companion bridge: {exc}")
        print()
        print("In the active aiplayerbot.conf set:")
        print(f"  AiPlayerbot.CommandServerPort = {DEFAULT_PORTS[0]}")
        print("Then restart mangosd and run setup again.")
        raise SystemExit(1)

    print(f"Connected to {host}:{port}")
    raw = send_request(host, port, "list", "0")
    bots = parse_bots(raw)

    if not bots:
        print("No online playerbots were found.")
        print("Log/summon the bot you want to use, then rerun setup.")
        raise SystemExit(1)

    print()
    print("Online playerbots:")
    for i, bot in enumerate(bots, 1):
        master = bot["master"] or "no active master"
        print(
            f"  {i}. {bot['name']} — level {bot['level']} {bot['class']} "
            f"(master: {master})"
        )

    print()
    while True:
        raw_choice = input("Choose companion number [1]: ").strip() or "1"
        try:
            index = int(raw_choice) - 1
            chosen = bots[index]
            break
        except (ValueError, IndexError):
            print("Please choose one of the listed numbers.")

    config = {
        "host": host,
        "port": port,
        "selector": chosen["guid"],
        "bot_name": chosen["name"],
    }
    CONFIG_PATH.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
    write_profile(chosen["name"])

    print()
    print(f"Selected {chosen['name']} (GUID {chosen['guid']}).")
    print(f"Saved {CONFIG_PATH.name}.")
    snapshot_raw = send_request(host, port, "snapshot", chosen["guid"])
    snapshot = parse_snapshot(snapshot_raw)
    print()
    print(json.dumps(snapshot, indent=2, ensure_ascii=False))
    print()
    print("Setup complete.")
    print("Open this folder in ChatGPT Desktop -> Codex and say:")
    print('  "Join me in WoW and control my companion."')


def main() -> int:
    parser = argparse.ArgumentParser(description="Playerbot Companion Bridge client")
    sub = parser.add_subparsers(dest="action", required=True)

    sub.add_parser("setup")
    sub.add_parser("snapshot")
    sub.add_parser("list")
    sub.add_parser("test")

    cmd = sub.add_parser("cmd")
    cmd.add_argument("text", nargs="+")

    party = sub.add_parser("party")
    party.add_argument("text", nargs="+")

    say = sub.add_parser("say")
    say.add_argument("text", nargs="+")

    raw = sub.add_parser("raw")
    raw.add_argument("text", nargs="+")

    args = parser.parse_args()

    if args.action == "setup":
        setup()
        return 0

    cfg = load_config()
    host = str(cfg.get("host", DEFAULT_HOST))
    port = int(cfg["port"])
    selector = str(cfg["selector"])

    if args.action == "snapshot":
        response = send_request(host, port, "snapshot", selector)
        print(json.dumps(parse_snapshot(response), indent=2, ensure_ascii=False))
    elif args.action == "list":
        response = send_request(host, port, "list", "0")
        print(json.dumps(parse_bots(response), indent=2, ensure_ascii=False))
    elif args.action == "test":
        version = send_request(host, port, "bridge-version", "0")
        snapshot = parse_snapshot(send_request(host, port, "snapshot", selector))
        print(f"bridge={version}")
        print(f"bot={snapshot.get('name', cfg.get('bot_name', selector))}")
        print("status=ok")
    elif args.action == "cmd":
        print(send_request(host, port, "cmd:" + " ".join(args.text), selector))
    elif args.action == "party":
        print(send_request(host, port, "party:" + " ".join(args.text), selector))
    elif args.action == "say":
        print(send_request(host, port, "say:" + " ".join(args.text), selector))
    elif args.action == "raw":
        print(send_request(host, port, " ".join(args.text), selector))
    else:
        raise AssertionError("unreachable")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
        print(f"bridge: {exc}", file=sys.stderr)
        raise SystemExit(1)
