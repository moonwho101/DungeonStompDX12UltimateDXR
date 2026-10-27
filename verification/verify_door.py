import os
from playwright.sync_api import sync_playwright

os.makedirs("/home/jules/verification/videos", exist_ok=True)
os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

def run_cuj(page):
    page.goto("http://localhost:8080/?run=1")

    # Wait for game world to finish loading doors
    page.wait_for_function("() => window.doors && window.doors.length > 0", timeout=10000)
    page.wait_for_timeout(500)

    # Click instructions to lock controls
    page.click("#instructions")
    page.wait_for_timeout(500)

    # Find first door position and move player in front of it looking at door
    page.evaluate("""() => {
        if (window.doors && window.doors.length > 0) {
            const door = window.doors[0];
            const pos = window.controls.getObject().position;
            pos.set(door.position.x, door.position.y, door.position.z + 120);
            window.camera.lookAt(door.position.x, door.position.y + 50, door.position.z);
        }
    }""")
    page.wait_for_timeout(500)

    # Press E button to open door
    page.keyboard.press("KeyE")

    # Wait until the door is fully open (currentUp === 200)
    page.wait_for_function("() => window.doors && window.doors[0].isOpen", timeout=15000)

    # Check door state
    door_state = page.evaluate("""() => {
        if (window.doors && window.doors.length > 0) {
            return {
                currentUp: window.doors[0].currentUp,
                isOpen: window.doors[0].isOpen,
                meshY: window.doors[0].mesh.position.y
            };
        }
        return null;
    }""")
    print("Door state after pressing E:", door_state)

    page.screenshot(path="/home/jules/verification/screenshots/verification.png")
    page.wait_for_timeout(1000)

if __name__ == "__main__":
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
