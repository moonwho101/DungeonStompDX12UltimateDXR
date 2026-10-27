import asyncio
import os
from playwright.async_api import async_playwright

async def run():
    async with async_playwright() as p:
        browser = await p.chromium.launch(
            headless=True,
            args=[
                '--use-gl=angle',
                '--use-angle=gl-egl',
                '--ignore-gpu-blocklist',
                '--disable-gpu-driver-bug-workarounds',
                '--enable-unsafe-webgpu'
            ]
        )
        page = await browser.new_page()

        errors = []
        page.on("pageerror", lambda err: errors.append(str(err)))
        page.on("console", lambda msg: print(f"CONSOLE [{msg.type}]: {msg.text}"))

        url = "http://localhost:8000/index.html?run=1"

        print(f"Navigating to {url}...")
        await page.goto(url)
        await page.wait_for_timeout(3000)

        # Click instructions overlay to hide it and start game
        await page.click("#instructions")
        await page.wait_for_timeout(1000)

        # Cycle weapons to reach MAGIC MISSILE (10 cycles from index 0)
        print("Cycling weapon to MAGIC MISSILE...")
        for _ in range(10):
            await page.keyboard.press("KeyQ")
            await page.wait_for_timeout(100)

        await page.wait_for_timeout(500)
        gun_text = await page.inner_text("#hud-gun-text")
        print(f"Current HUD Weapon: {gun_text}")

        # Fire MAGIC MISSILE
        print("Firing MAGIC MISSILE...")
        await page.mouse.click(400, 300)
        await page.wait_for_timeout(300)

        os.makedirs("verification/screenshots", exist_ok=True)
        screenshot_path = "verification/screenshots/magic_missile.png"
        await page.screenshot(path=screenshot_path)
        print(f"Saved screenshot: {screenshot_path}")

        # Cycle to FIREBALL
        await page.keyboard.press("KeyQ")
        await page.wait_for_timeout(300)
        gun_text = await page.inner_text("#hud-gun-text")
        print(f"Current HUD Weapon: {gun_text}")

        # Fire FIREBALL
        print("Firing FIREBALL...")
        await page.mouse.click(400, 300)
        await page.wait_for_timeout(300)
        await page.screenshot(path="verification/screenshots/fireball.png")

        # Cycle to LIGHTNING
        await page.keyboard.press("KeyQ")
        await page.wait_for_timeout(300)
        gun_text = await page.inner_text("#hud-gun-text")
        print(f"Current HUD Weapon: {gun_text}")

        # Fire LIGHTNING
        print("Firing LIGHTNING...")
        await page.mouse.click(400, 300)
        await page.wait_for_timeout(300)
        await page.screenshot(path="verification/screenshots/lightning.png")

        # Cycle to HEALING
        await page.keyboard.press("KeyQ")
        await page.wait_for_timeout(300)
        gun_text = await page.inner_text("#hud-gun-text")
        print(f"Current HUD Weapon: {gun_text}")

        # Cast HEALING
        print("Casting HEALING...")
        await page.mouse.click(400, 300)
        await page.wait_for_timeout(300)
        await page.screenshot(path="verification/screenshots/healing.png")

        print("Page errors during execution:", errors)
        assert len(errors) == 0, f"Page errors encountered: {errors}"

        await browser.close()

if __name__ == "__main__":
    asyncio.run(run())
