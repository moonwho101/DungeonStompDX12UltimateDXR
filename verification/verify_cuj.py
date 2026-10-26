import os
from playwright.sync_api import sync_playwright

os.makedirs("/home/jules/verification/videos", exist_ok=True)
os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

def run_cuj(page):
    page.goto("http://localhost:8000/index.html")
    page.wait_for_timeout(1000)

    # Click instructions to lock pointer and start
    page.click("#instructions")
    page.wait_for_timeout(1000)

    # Press G to generate new level
    page.keyboard.press("KeyG")
    page.wait_for_timeout(2500)

    # Press W to move forward into new dungeon
    page.keyboard.down("KeyW")
    page.wait_for_timeout(1500)
    page.keyboard.up("KeyW")
    page.wait_for_timeout(500)

    # Cycle weapons
    page.keyboard.press("KeyQ")
    page.wait_for_timeout(500)
    page.keyboard.press("KeyQ")
    page.wait_for_timeout(500)

    page.screenshot(path="/home/jules/verification/screenshots/verification.png")
    page.wait_for_timeout(1000)

if __name__ == "__main__":
    with sync_playwright() as p:
        browser = p.chromium.launch(
            headless=True,
            args=[
                '--use-gl=angle',
                '--use-angle=gl-egl',
                '--ignore-gpu-blocklist',
                '--disable-gpu-driver-bug-workarounds',
                '--enable-unsafe-webgpu'
            ]
        )
        context = browser.new_context(
            record_video_dir="/home/jules/verification/videos"
        )
        page = context.new_page()
        try:
            run_cuj(page)
        finally:
            context.close()
            browser.close()
