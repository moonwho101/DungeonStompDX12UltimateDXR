import os
import time
import subprocess
from playwright.sync_api import sync_playwright

os.makedirs("/home/jules/verification/videos", exist_ok=True)
os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

def run_verification():
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
                page.goto("http://localhost:8080")
                page.wait_for_function("() => window.monsters && window.monsters.length > 0", timeout=10000)
                page.wait_for_timeout(500)

                # Press instructions to unlock audio
                page.click("#instructions")
                page.wait_for_timeout(500)

                # Press 'KeyP' to toggle sound off
                page.keyboard.press("KeyP")
                page.wait_for_timeout(1000)

                # Press 'KeyP' again to toggle sound back on
                page.keyboard.press("KeyP")
                page.wait_for_timeout(1000)

                # Press 'KeyM' to toggle music
                page.keyboard.press("KeyM")
                page.wait_for_timeout(1000)

                # Take screenshot
                page.screenshot(path="/home/jules/verification/screenshots/sound_toggle_verification.png")
                page.wait_for_timeout(1000)

            finally:
                context.close()
                browser.close()
    finally:
        server_process.terminate()

if __name__ == "__main__":
    run_verification()
