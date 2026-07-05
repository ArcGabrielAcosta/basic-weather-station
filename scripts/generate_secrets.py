#!/usr/bin/env python3
"""Genera secrets.h a partir de .env para el sketch de Arduino."""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENV_FILE = ROOT / ".env"
OUT_FILE = ROOT / "secrets.h"

REQUIRED = (
    "WIFI_SSID",
    "WIFI_PASSWORD",
    "MQTT_BROKER",
    "MQTT_PORT",
    "MQTT_USER",
    "MQTT_PASSWORD",
    "MQTT_CLIENT_ID",
)


def load_env(path: Path) -> dict[str, str]:
    if not path.is_file():
        raise SystemExit(
            f"No existe {path}\n"
            "Creá .env a partir de .env.example:\n"
            "  cp .env.example .env"
        )

    env: dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        env[key.strip()] = value.strip().strip('"').strip("'")
    return env


def c_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def normalize_broker(url: str) -> str:
    url = url.strip()
    for prefix in ("mqtts://", "mqtt://", "ssl://", "https://", "http://"):
        if url.lower().startswith(prefix):
            url = url[len(prefix) :]
    return url.rstrip("/")


def main() -> None:
    env = load_env(ENV_FILE)

    missing = [key for key in REQUIRED if not env.get(key)]
    if missing:
        raise SystemExit(f"Faltan variables en .env: {', '.join(missing)}")

    try:
        mqtt_port = int(env["MQTT_PORT"])
    except ValueError as exc:
        raise SystemExit("MQTT_PORT debe ser un numero entero") from exc

    broker = normalize_broker(env["MQTT_BROKER"])

    content = f"""#pragma once
// Generado automaticamente desde .env — no editar a mano.
// Regenerar con: python3 scripts/generate_secrets.py

#define WIFI_SSID "{c_string(env['WIFI_SSID'])}"
#define WIFI_PASSWORD "{c_string(env['WIFI_PASSWORD'])}"

#define MQTT_BROKER "{c_string(broker)}"
#define MQTT_PORT {mqtt_port}
#define MQTT_USER "{c_string(env['MQTT_USER'])}"
#define MQTT_PASSWORD "{c_string(env['MQTT_PASSWORD'])}"
#define MQTT_CLIENT_ID "{c_string(env['MQTT_CLIENT_ID'])}"
"""

    OUT_FILE.write_text(content, encoding="utf-8")
    print(f"OK: {OUT_FILE} generado desde {ENV_FILE}")


if __name__ == "__main__":
    main()
