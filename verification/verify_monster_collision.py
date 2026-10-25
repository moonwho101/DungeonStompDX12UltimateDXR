import os
import time
from playwright.sync_api import sync_playwright

os.makedirs("/home/jules/verification/screenshots", exist_ok=True)

def test_monster_gravity_and_collision(page):
    page.goto("http://localhost:8080")

    # Wait for game world to finish loading monsters
    page.wait_for_function("() => window.monsters && window.monsters.length > 0", timeout=10000)
    page.wait_for_timeout(500)

    # Verify initial monsters are grounded on floor geometry
    initial_states = page.evaluate("""() => {
        return {
            m0_y: window.monsters[0].position.y,
            m0_grounded: window.monsters[0].isGrounded
        };
    }""")
    print("Initial Monster 0 state on load:", initial_states)
    assert initial_states["m0_grounded"] == True, "Monster 0 should be grounded on map load!"

    # 1. Test Monster Gravity
    # Teleport first monster up in the air at y = 250
    initial_y = page.evaluate("""() => {
        const m = window.monsters[0];
        m.position.y = 250.0;
        m.velocity.y = 0.0;
        m.isGrounded = false;
        return m.position.y;
    }""")
    print("Monster set to high Y height:", initial_y)

    # Wait until monster falls and becomes grounded
    page.wait_for_function("() => window.monsters && window.monsters[0].isGrounded === true", timeout=10000)

    # Check monster Y height and isGrounded status after gravity
    grounded_state = page.evaluate("""() => {
        const m = window.monsters[0];
        return {
            y: m.position.y,
            isGrounded: m.isGrounded,
            vy: m.velocity.y
        };
    }""")
    print("Monster state after falling to floor under gravity:", grounded_state)

    assert grounded_state["isGrounded"] == True, f"Monster is not grounded! isGrounded={grounded_state['isGrounded']}"
    assert grounded_state["y"] <= initial_states["m0_y"] + 5.0, f"Monster Y did not drop back to floor level! Y={grounded_state['y']}"

    # 2. Test Monster 3DS/MD2 Model Collision (Side collision pushing)
    collision_test = page.evaluate("""() => {
        const m = window.monsters[0];
        // Find a valid 3DS/MD2 model box
        const box = window.modelBoxes ? window.modelBoxes.find(b => b !== m.boundingBox && !b.isEmpty()) : null;
        if (!box) return { tested: false };

        // Position monster slightly penetrating the side of the box
        const startX = box.min.x + 5;
        const centerZ = (box.min.z + box.max.z) * 0.5;
        m.position.x = startX;
        m.position.y = box.min.y;
        m.position.z = centerZ;

        // Run capsule collision resolution
        window.resolveMonsterCapsuleCollisions(m);

        // Position should be pushed out to outside box.min.x
        return {
            tested: true,
            boxMinX: box.min.x,
            boxMaxX: box.max.x,
            startX: startX,
            finalX: m.position.x,
            pushedOut: m.position.x < box.min.x || m.position.x > box.max.x
        };
    }""")
    print("Model side collision test result:", collision_test)

    if collision_test["tested"]:
        assert collision_test["pushedOut"], f"Monster was not pushed out horizontally! Final X={collision_test['finalX']}, Box minX={collision_test['boxMinX']}"

    # Move player in front of monster and model, set camera view
    page.evaluate("""() => {
        const m = window.monsters[0];
        const pos = window.controls.getObject().position;
        pos.set(m.position.x, m.position.y + 100, m.position.z + 250);
        window.camera.lookAt(m.position.x, m.position.y + 30, m.position.z);
    }""")

    page.wait_for_timeout(500)
    screenshot_path = "/home/jules/verification/screenshots/monster_verification.png"
    page.screenshot(path=screenshot_path)
    print("Saved screenshot to:", screenshot_path)

if __name__ == "__main__":
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        try:
            test_monster_gravity_and_collision(page)
            print("Monster gravity and collision verification PASSED successfully!")
        finally:
            browser.close()
