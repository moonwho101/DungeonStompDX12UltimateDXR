import os
import time
import subprocess
from playwright.sync_api import sync_playwright

os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

def verify_sound_features():
    # Start python HTTP server if not running
    server_process = None
    try:
        import urllib.request
        urllib.request.urlopen("http://localhost:8080")
    except Exception:
        server_process = subprocess.Popen(["python3", "-m", "http.server", "8080"])
        time.sleep(1)

    console_errors = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()

        page.on("console", lambda msg: console_errors.append(msg.text) if msg.type == "error" else None)

        page.goto("http://localhost:8080")

        # Wait for game world to load
        page.wait_for_function("() => window.monsters && window.monsters.length > 0", timeout=10000)
        page.wait_for_timeout(500)

        # Check export variables and constants
        audio_checks = page.evaluate("""() => {
            return {
                hasPlaySound: typeof window.playSound === 'function' || typeof playSound !== 'undefined',
                hasSwishSounds: Array.isArray(window.SWISH_SOUNDS),
                hasImpactSounds: Array.isArray(window.IMPACT_SOUNDS),
                hasPlayerImpactSounds: Array.isArray(window.PLAYER_IMPACT_SOUNDS),
                monsterConfigSound: window.monsters[0].config.sound !== undefined,
                monster0Sound: window.monsters[0].config.sound,
                monstersCount: window.monsters.length
            };
        }""")
        print("Audio Checks:", audio_checks)
        assert audio_checks["monstersCount"] > 0, "Monsters should be loaded in scene"
        assert audio_checks["monsterConfigSound"] == True, "Monster config should have sound assigned"

        # Test triggering attack swish sound
        page.evaluate("""() => {
            if (typeof triggerWeaponAttack === 'function') {
                triggerWeaponAttack();
            }
        }""")

        # Test triggering player damage impact sound
        page.evaluate("""() => {
            if (typeof damagePlayer === 'function') {
                damagePlayer(5);
            }
        }""")

        # Test triggering monster noise
        page.evaluate("""() => {
            if (window.monsters && window.monsters[0]) {
                window.monsters[0].playMonsterNoise(100.0);
            }
        }""")

        page.wait_for_timeout(500)

        # Verify no severe JS errors occurred
        print("Console errors logged:", console_errors)

        screenshot_path = "verification/screenshots/sound_verification.png"
        page.screenshot(path=screenshot_path)
        print("Saved verification screenshot to:", screenshot_path)

        browser.close()

    if server_process:
        server_process.terminate()

if __name__ == "__main__":
    verify_sound_features()
