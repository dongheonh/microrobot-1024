![Group 104](https://github.com/user-attachments/assets/230589e6-b947-4003-9ae3-6c34a064bb6e)

# microrobot-1024

**Authors:** Dong Heon Han (SAM Lab)

**The first project on a 1024-magnet array!**  
A scalable 1024-channel electromagnetic array system for programmable collective robotics.

---

## Overview

This project implements a 1024-magnet array for high-resolution magnetic field control.  
The system is designed for scalability, modularity, and real-time operation.

---

## Repository Structure

```text
firmware/       # Embedded code running on the microcontrollers (Raspberry Pi Pico 1 / Pico 2)
hardware/       # CAD models, PCB layouts, and schematics
software/       # Host-side communication protocols between PC and Pico
documents/      # Design notes, protocols, and system-level documentation
models/         # Analytical magnetic field and system models
simulations/    # Offline numerical simulations based on the models
