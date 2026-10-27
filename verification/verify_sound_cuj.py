import os
import subprocess
import time
from playwright.sync_api import sync_playwright

os.makedirs("/home/jules/verification/videos", exist_ok=True)
os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

def run_cuj(page):
    page.goto("http://localhost:8080/?run=1")
    page.wait_for_timeout(1000)

    # Wait for game world and monsters to load
    page.wait_for_function("() => window.monsters && window.monsters.length > 0", timeout=10000)
    page.wait_for_timeout(1000)

    # Inspect monster configs and sound properties
    audio_info = page.evaluate("""() => {
        const m = window.monsters[0];
        return {
            name: m.config.name,
            painSound: m.config.painSound,
            deathSound: m.config.deathSound,
            weaponSound: m.config.weaponSound,
            yellSound: m.config.yellSound,
            sound: m.config.sound
        };
    }""")
    print("Monster 0 Audio Config:", audio_info)

    # Trigger monster noise (yellSound)
    page.evaluate("""() => {
        if (window.monsters && window.monsters[0]) {
            window.monsters[0].playMonsterNoise(100.0);
        }
    }""")
    page.wait_for_timeout(1000)

    # Trigger player damage
    page.evaluate("""() => {
        if (typeof damagePlayer === 'function') {
            damagePlayer(10);
        }
    }""")
    page.wait_for_timeout(1000)

    # Trigger weapon attack
    page.evaluate("""() => {
        if (typeof triggerWeaponAttack === 'function') {
            triggerWeaponAttack();
        }
    }""")
    page.wait_for_timeout(1000)

    # Take screenshot at the end
    page.screenshot(path="/home/jules/verification/screenshots/sound_verification_final.png")
    page.wait_for_timeout(1000)

if __name__ == "__main__":
    server_process = None
    try:
        import urllib.request
        urllib.request.urlopen("http://localhost:8080")
    except Exception:
        server_process = subprocess.Popen(["python3", "-m", "http.server", "8080"])
        time.sleep(1)

    try:
        with sync_playwright() as p:
            browser = p.chromium.launch(headless=True)
            context = browser.new_context(
                record_video_dir="/home/jules/verification/videos"
            )
            page = context.new_page()
            try:
                run_cuj(page)
            finally:
                context.close()
                browser.close()
    finally:
        if server_process:
            server_process.terminate()
