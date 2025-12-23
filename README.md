# HTS-Time: Human Time System Universal Tool

**Version 1.2.2**

HTS-Time is a professional-grade chronological utility designed for the **Human Time System (HTS)**—a time notation system built for an absolute 24-billion-year lifespan. This tool provides a seamless bridge between Gregorian AD and HTS notation through both a gorgeous Graphical User Interface and a high-performance Command Line Interface.



## 🚀 Key Features

### 🕒 The "Fire" Time Dashboard (GUI)
* **Tri-Clock Synchronization:** Simultaneously view HTS Era notation, UTC System Time, and Local Time.
* **Smart Color Coding:** Visual distinction between UTC (Blue) and Local Time (Vibrant Green) for instant readability.
* **Theme Engine:** Persistent support for five visual modes: Dark, Light, Sepia, Slate, and Neon.

### 💻 Universal CLI
* **Native Feel:** The `hts-time` command mimics the standard Linux `date` output but appends HTS notation.
* **Context Aware:** One binary handles everything. Type `hts-time` for text or `hts-time --gui` to launch the window.
* **Integrated Manual:** Full documentation available via `man hts-time`.

### 🛠️ Professional Engineering
* **Flatpak Ready:** Optimized for GNOME 46 and fully sandboxed for security.
* **Setup Utility:** Integrated "Help Hub" generates robust terminal alias scripts for you.
* **Low Overhead:** Built in pure C with GTK4 for maximum performance and minimum resource usage.



## 📥 Installation

### Building from Source
Ensure you have `meson`, `ninja`, and the `GTK4` development headers installed.

1. **Clone the repository:**
   ```bash
   git clone [https://github.com/storcke64/hts-time.git](https://github.com/storcke64/hts-time.git)
   cd hts-time
