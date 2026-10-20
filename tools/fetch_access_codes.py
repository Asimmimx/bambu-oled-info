#!/usr/bin/env python3
"""Reads the LAN access codes of your printers from your Bambu account, once.

Why: the A1 / A1 mini only show their access code on the printer after "LAN Only Mode" is turned
on, which cuts the printer off from Bambu Cloud. Your account already knows the code, so this
script asks the account instead. Afterwards the monitor talks to the printers directly over your
LAN and never needs the cloud.

What it does
  1. logs in to Bambu Cloud (password typed at the prompt; an e-mail / 2FA code if Bambu asks);
  2. lists the printers bound to the account (name, serial, model, online);
  3. writes each printer's access code into include/secrets.h, matching on the serial number;
  4. optionally exports printers that are not in secrets.h (--export-others), e.g. the ones in
     another house, to a separate git-ignored file.

What it does not do: it never prints or stores your password or token, and it only sends them to
Bambu's own servers (api.bambulab.com). It uses an honest client name, not the slicer's.
This talks to Bambu's unofficial cloud API; Bambu may change it at any time.

Usage:  python tools/fetch_access_codes.py [--email you@example.com] [--cn] [--export-others]
        python tools/fetch_access_codes.py --token [--export-others]

--token skips the login: if you are already signed in to makerworld.com (same Bambu account), copy
the value of its "token" cookie (browser DevTools -> Application -> Cookies) and paste it at the
prompt. Treat that token like a password: it gives full access to your Bambu account for ~3 months.

Needs only the Python standard library.
"""
import argparse
import getpass
import json
import re
import shutil
import sys
import urllib.error
import urllib.request
from pathlib import Path

USER_AGENT = "bambu-oled-info/0.1 (personal status display)"
REGIONS = {
    "global": ("https://api.bambulab.com", "https://bambulab.com"),
    "cn": ("https://api.bambulab.cn", "https://bambulab.cn"),
}


def fail(message):
    print("\nERROR: " + message, file=sys.stderr)
    sys.exit(1)


def call(url, body=None, token=None):
    """GET (body is None) or POST JSON. Returns (parsed JSON or {}, response headers)."""
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method="POST" if body is not None else "GET")
    req.add_header("User-Agent", USER_AGENT)
    req.add_header("Accept", "application/json")
    if body is not None:
        req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(req, timeout=20) as resp:
            raw, headers = resp.read().decode("utf-8", "replace"), resp.headers
    except urllib.error.HTTPError as e:
        fail("HTTP %d from %s\n%s" % (e.code, url, e.read().decode("utf-8", "replace")[:300]))
    except urllib.error.URLError as e:
        fail("Could not reach %s (%s)" % (url, e.reason))
    try:
        return json.loads(raw), headers
    except ValueError:
        return {}, headers


def login(api, web, email):
    password = getpass.getpass("Bambu account password (typed here, never stored): ")
    data, _ = call(api + "/v1/user-service/user/login", {"account": email, "password": password})
    password = None
    if data.get("accessToken"):
        return data["accessToken"]

    kind = data.get("loginType")
    if kind == "verifyCode":
        call(api + "/v1/user-service/user/sendemail/code", {"email": email, "type": "codeLogin"})
        print("Bambu e-mailed you a verification code.")
        code = input("Verification code: ").strip()
        data, _ = call(api + "/v1/user-service/user/login", {"account": email, "code": code})
        if data.get("accessToken"):
            return data["accessToken"]
    elif kind == "tfa":
        code = input("Authenticator app (2FA) code: ").strip()
        data, headers = call(web + "/api/sign-in/tfa", {"tfaKey": data.get("tfaKey"), "tfaCode": code})
        if data.get("accessToken"):
            return data["accessToken"]
        for cookie in headers.get_all("Set-Cookie") or []:
            if cookie.startswith("token="):
                return cookie.split(";", 1)[0][len("token="):]

    fail("Login did not return a token (loginType=%r, message=%r). Wrong password or code?"
         % (data.get("loginType"), data.get("message")))


def short_name(name):
    letters = re.sub(r"[^A-Za-z0-9]", "", name).upper()
    return letters[:5] or "PRN"


def update_secrets(path, devices):
    """Fills in accessCode for every device whose serial already appears in secrets.h."""
    text = path.read_text(encoding="utf-8")
    shutil.copy(path, path.with_name(path.name + ".bak"))
    matched, others = [], []
    for dev in devices:
        serial, code = dev["dev_id"], dev["dev_access_code"]
        if '"%s"' % serial not in text:
            others.append(dev)
            continue
        pattern = re.compile(r'("%s"\s*,\s*")[^"]*(")' % re.escape(serial))
        text = pattern.sub(lambda m: m.group(1) + code + m.group(2), text)
        matched.append(dev)
    path.write_text(text, encoding="utf-8")
    return matched, others


def export_others(path, devices):
    lines = [
        "// Printers on your Bambu account that are not in secrets.h (e.g. the ones in another house).",
        "// Copy the lines you need into PRINTERS[] of the unit that lives next to those printers",
        "// and fill in their LAN address. This file is git-ignored.",
    ]
    for dev in devices:
        lines.append('    {"%s", "%s", "192.168.x.x", "%s", "%s"},'
                     % (dev["name"], short_name(dev["name"]), dev["dev_id"], dev["dev_access_code"]))
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(description="Fetch printer LAN access codes from your Bambu account.")
    ap.add_argument("--email", help="Bambu account e-mail (asked if omitted)")
    ap.add_argument("--cn", action="store_true", help="China-region account (bambulab.cn)")
    ap.add_argument("--token", action="store_true",
                    help="paste the token cookie of an existing makerworld.com session instead of logging in")
    ap.add_argument("--secrets", default=str(root / "include" / "secrets.h"), help="secrets.h to update")
    ap.add_argument("--export-others", action="store_true",
                    help="write printers missing from secrets.h to include/secrets.others.h")
    args = ap.parse_args()

    api, web = REGIONS["cn" if args.cn else "global"]
    secrets = Path(args.secrets)
    if not secrets.exists():
        fail("%s not found. Copy include/secrets.example.h to include/secrets.h first." % secrets)

    if args.token:
        token = getpass.getpass("Paste the 'token' cookie value (not shown, never stored): ").strip().strip('"')
        if token.lower().startswith("bearer "):
            token = token[7:].strip()
        if not token:
            fail("No token entered.")
    else:
        email = args.email or input("Bambu account e-mail: ").strip()
        token = login(api, web, email)

    data, _ = call(api + "/v1/iot-service/api/user/bind", token=token)
    devices = [d for d in data.get("devices", []) if d.get("dev_id")]
    for d in devices:
        d["dev_access_code"] = (d.get("dev_access_code") or "").strip()
        d["name"] = d.get("name") or d["dev_id"]
    if not devices:
        fail("The account has no printers bound to it.")

    print("\nPrinters on your account:")
    print("  %-18s %-17s %-16s %s" % ("Name", "Serial", "Model", "Online"))
    for d in devices:
        model = d.get("dev_product_name") or d.get("dev_model_name") or "?"
        print("  %-18s %-17s %-16s %s" % (d["name"][:18], d["dev_id"], model[:16], "yes" if d.get("online") else "no"))

    missing = [d["name"] for d in devices if not d["dev_access_code"]]
    devices = [d for d in devices if d["dev_access_code"]]
    matched, others = update_secrets(secrets, devices)

    print("\nAccess codes written to %s (backup: %s.bak):" % (secrets, secrets.name))
    print("  " + (", ".join(d["name"] for d in matched) or "none: no serial in secrets.h matches the account"))
    if missing:
        print("\nNo access code returned for: " + ", ".join(missing))
    if others:
        print("\nOn the account but not in secrets.h: " + ", ".join(d["name"] for d in others))
        if args.export_others:
            out = secrets.with_name("secrets.others.h")
            export_others(out, others)
            print("  Exported with their codes to %s" % out)
        else:
            print("  Re-run with --export-others to save them (with their codes) for another unit.")
    print("\nDone. Nothing was stored except the codes in secrets.h; the token was discarded.")


if __name__ == "__main__":
    main()
