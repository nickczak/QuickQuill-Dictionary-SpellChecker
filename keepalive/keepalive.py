#!/usr/bin/env python3
"""Keep the free QuickQuill backend instance awake on Render.

Render free web services sleep after ~15 minutes without traffic and slow to
wake. This cron job (deployed by render.yaml) pings GET /api/health a few
times in quick succession every 10 minutes so the instance stays warm.
"""

import os
import sys
import time
from urllib.request import Request, urlopen

BASE_URL = os.environ.get("PING_URL") or (sys.argv[1] if len(sys.argv) > 1 else "")
if not BASE_URL:
    print("PING_URL is not set; nothing to ping", file=sys.stderr)
    sys.exit(1)

HEALTH_URL = f"{BASE_URL.rstrip('/')}/api/health"

for attempt in range(3):
    try:
        with urlopen(
            Request(HEALTH_URL, headers={"User-Agent": "quickquill-keepalive"}),
            timeout=30,
        ) as resp:
            print(f"ping #{attempt + 1}: HTTP {resp.status}")
            if resp.status == 200:
                sys.exit(0)
    except Exception as err:  # noqa: BLE001 — keep going on cold starts
        print(f"ping #{attempt + 1} failed: {err}")
    if attempt < 2:
        time.sleep(5)

sys.exit(1)