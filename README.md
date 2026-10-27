# ESP32 Button + LED Controller (Bluetooth + Supabase)

A physical button on the ESP32 sends a message to your phone, a button in the phone app lights the LED,
and every event is saved per user in Supabase.

```
[Button] -> ESP32 --BLE notify--> Phone web app --> Supabase (counts per user)
[LED]    <- ESP32 <--BLE write--- Phone web app
```

```
esp32_project/
  firmware/stage1_serial/stage1_serial.ino   Optional warm-up: serial only
  firmware/stage2_ble/stage2_ble.ino         The real firmware (Bluetooth)
  phone_app/index.html                       The phone app (single file)
  phone_app/supabase_setup.sql               Database schema
```

---

## 0. What you need

- ESP32 dev board (classic ESP32, S3, or C3. **Not** the S2, which has no Bluetooth)
- USB cable that carries **data** (some cables are charge-only)
- 1 push button, 1 LED + 220 ohm resistor (optional, the onboard LED works), breadboard, jumper wires
- Android phone with Chrome, or iPhone with the free **Bluefy** browser (Safari does not support Web Bluetooth)
- Free accounts: **Supabase**, and **Netlify** (or GitHub) for hosting
- A computer with the **Arduino IDE 2.x** (arduino.cc/en/software)

---

## 1. Wire it up

| Part | Connection |
|---|---|
| Button leg 1 | GPIO 4 |
| Button leg 2 | GND |
| LED (optional external) | GPIO 2 -> 220 ohm resistor -> LED long leg; LED short leg -> GND |

No resistor is needed for the button: the code enables the internal pull-up.
GPIO 2 is the onboard LED on most dev boards, so you can skip the external LED at first.

---

## 2. Set up the Arduino IDE

1. Open **File -> Preferences**. In **Additional boards manager URLs** add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
2. Open **Tools -> Board -> Boards Manager**, search **esp32**, and install **esp32 by Espressif Systems**.
3. Plug in the board. Select **Tools -> Board -> esp32 -> ESP32 Dev Module** (or your exact board).
4. Select **Tools -> Port**. If no port appears, install the USB driver for your board's chip
   (CP210x or CH340) and try a different cable.
5. Set **Tools -> Partition Scheme -> Huge APP (3MB No OTA/1MB SPIFFS)**. BLE sketches are large.
6. Do not install any separate "BLE" library. BLE is built into the ESP32 core, and an extra one causes conflicts.

---

## 3. Warm-up: test the wiring (optional but recommended)

1. Open `firmware/stage1_serial/stage1_serial.ino` and click **Upload**.
   (If the upload hangs on "Connecting...", hold the board's **BOOT** button until it starts.)
2. Open **Tools -> Serial Monitor** at **115200 baud**.
3. Press the button: you should see `Button pressed!`.
4. Type `t` and press Enter: the LED toggles. `1` is on, `0` is off.

If this works, your wiring and toolchain are good.

---

## 4. Flash the Bluetooth firmware

1. Open `firmware/stage2_ble/stage2_ble.ino` and **Upload**. (The folder name must match the file name.)
2. Serial Monitor should print: `Advertising as 'ESP32-Demo'`.

**Test it with nRF Connect** (free phone app) before touching the web app:

1. Scan, then **Connect** to `ESP32-Demo`.
2. Expand the **Unknown Service** (`4fafc201-...`).
3. On the characteristic ending `...26a8` (NOTIFY), tap the notify icon (stacked down arrows). Press the physical button:
   `Button pressed!` should appear.
4. On the characteristic starting `d1e2f3a4` (WRITE), tap the up arrow, choose **Text/UTF-8**, send `1`, `0`, or `t`. The LED should respond.
5. **Disconnect** in nRF Connect before continuing. The ESP32 only accepts one connection at a time.

If a characteristic seems to be missing, refresh services or toggle your phone's Bluetooth to clear the cache.

---

## 5. Set up Supabase

1. Create a project at supabase.com (note the database password, though this app doesn't need it).
2. Open **SQL Editor -> New query**, paste all of `phone_app/supabase_setup.sql`, and **Run**.
   You should see "Success". Check **Table Editor** for `button_events`.
3. Open **Authentication -> Providers -> Email**. For testing, turn **off** "Confirm email" so new accounts work immediately.
   (Turn it back on for real use.)
4. Open **Project Settings -> API** and copy:
   - **Project URL**
   - **anon public** key (never use the `service_role` key in the app)

---

## 6. Configure and host the phone app

1. Open `phone_app/index.html` in a text editor. Near the top of the script, replace:
   ```js
   const SUPABASE_URL      = "YOUR_SUPABASE_URL";
   const SUPABASE_ANON_KEY = "YOUR_SUPABASE_ANON_KEY";
   ```
2. Web Bluetooth requires **HTTPS**, so the file must be hosted. Easiest option:
   - Go to **app.netlify.com/drop** and drag the `phone_app` folder onto the page.
   - Netlify gives you an `https://...netlify.app` link.
   - Alternative: create a GitHub repo, add `index.html`, and enable **Settings -> Pages**.
3. Supabase needs to allow your site for sign-in redirects only if you use magic links or email confirmation.
   Password login works without extra configuration. If you turn confirmation on, add your site URL under
   **Authentication -> URL Configuration**.
4. After editing the file, redeploy (drag the folder onto Netlify again).

---

## 7. Use it

1. On your phone, turn on **Bluetooth** (and **Location** on Android, which Chrome needs for scanning).
2. Open your hosted link in **Chrome** (Android) or **Bluefy** (iPhone).
3. **Sign up** with an email and password, then **Sign in**.
4. Tap **Connect** and choose `ESP32-Demo`.
5. Press the physical button: the banner flashes, the phone vibrates, and **Button presses** goes up.
6. Tap **On / Off / Toggle**: the LED responds and **LED commands** goes up.

### Verify the data
In Supabase **Table Editor -> button_events** you should see a row for every event, tagged with your `user_id`.
Create a second account and confirm each user only sees their own counts.

---

## 8. Troubleshooting

| Symptom | Fix |
|---|---|
| `exit status 1` on compile | Get the real error: **File -> Preferences -> Show verbose output during compilation**. Check the board is an ESP32, remove extra BLE libraries, set the Huge APP partition scheme. |
| Upload stuck on "Connecting..." | Hold **BOOT** while uploading; try another USB cable or port. |
| No notify icon in nRF Connect | Look under the `4fafc201` service, characteristic `...26a8`; refresh services / toggle phone Bluetooth. |
| App says Web Bluetooth unavailable | Use Chrome on Android or Bluefy on iPhone. Page must be served over HTTPS. |
| Device not found when scanning | Close nRF Connect or any other app connected to the ESP32; reset the board. |
| Disconnects, can't reconnect | Reset the ESP32 (the sketch restarts advertising on disconnect, but a reset is a quick fix). |
| "Save failed" / row-level security error | Make sure you ran the whole SQL file and are signed in. |
| Sign-up succeeds but can't sign in | Email confirmation is on; confirm via email or disable it for testing. |
| Counts don't increase | You must be **signed in** and **connected**; the ESP32 itself has no internet, so presses while no phone is connected are not recorded. |

---

## 9. Ideas for next steps

- FIND AND FIX BUTTON PRESS BUG that prevents button presses from being logged on web app
- ESP32 reports LED state back, so the app always shows the true state
- History chart of presses over time
- Leaderboard across users
- ESP32 joins WiFi and posts events to Supabase directly (counts presses with no phone nearby)
- Brightness slider using PWM
