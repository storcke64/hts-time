# HTS System Command Reference
**Version:** 1.4 (Smart-Align Build)
**Scope:** Screensaver, Dashboard, and Calibration Modules

---

## 🔧 Hardware Calibration Mode
*Used to align the "Furniture" (Dashboard) with the physical bezel of the display monitor.*

| Key Combination | Action | Description |
| :--- | :--- | :--- |
| **CTRL + C** | **Toggle Mode** | Activates/Deactivates Calibration Mode. <br>_Visual Cue: Top border turns RED when active._ |
| **ENTER** | **Save & Exit** | Exits Calibration Mode and saves the current offset to memory. |
| **ARROW UP** | **Nudge Up** | Moves the dashboard up by **1 pixel** (Fine-Tune). |
| **ARROW DOWN** | **Nudge Down** | Moves the dashboard down by **1 pixel** (Fine-Tune). |
| **SHIFT + UP** | **Lift Up** | Moves the dashboard up by **10 pixels** (Fast Adjust). |
| **SHIFT + DOWN** | **Drop Down** | Moves the dashboard down by **10 pixels** (Fast Adjust). |

> **Note:** These commands only function while on the `screensaver.html` page. The system prevents page scrolling while Calibration Mode is active to ensure precision.

---

## 💾 System Memory
*These values are stored persistently in the LocalStorage of the Flatpak sandbox.*

* **`hts_calibrated_height`** / **`hts_user_offset`**: Stores the pixel offset defined by the user during calibration.
* **`hts_theme_preference`**: Stores the file path of the last selected Theme Deck (e.g., `theme_industrial.html`).
* **`hts_active_idx`**: Stores the index of the currently playing video background.

---

## 🖥️ Auto-Fit Behavior
*Passive logic that runs automatically on load.*

* **Theme Scan:** Upon loading a new theme, the system scans for 2 seconds to detect the content height.
* **Snap-to-Fit:** If the theme is taller than the default, the dashboard container automatically expands upwards to fit the new content.
* **Offset Application:** Any user calibration (via **CTRL+C**) is applied *on top* of this auto-detected height.
