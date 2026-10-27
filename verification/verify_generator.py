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
        await page.wait_for_timeout(2000)

        # Click instructions to lock
        await page.click("#instructions")
        await page.wait_for_timeout(1000)

        initial_objects = await page.inner_text("#obj-display")
        print(f"Initial Object Count: {initial_objects}")

        # Press KeyG to generate a new level
        print("Pressing KeyG to generate new level...")
        await page.keyboard.press("KeyG")
        await page.wait_for_timeout(3000)

        new_objects = await page.inner_text("#obj-display")
        print(f"New Object Count: {new_objects}")

        os.makedirs("verification/screenshots", exist_ok=True)
        screenshot_path = "verification/screenshots/generated_level.png"
        await page.screenshot(path=screenshot_path)
        print(f"Saved screenshot: {screenshot_path}")

        print("Page errors during execution:", errors)
        assert len(errors) == 0, f"Page errors encountered: {errors}"

        await browser.close()

if __name__ == "__main__":
    asyncio.run(run())
