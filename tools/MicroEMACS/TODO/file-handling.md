# File Handling

---

## Binary Introspection Engine: Integrated C Hexl Mode
To facilitate hardware architecture training and bare-metal executable verification, a low-overhead view conversion layer has been hardcoded into the text-rendering pipelines.
* **Stream-Filter Extraction:** Bypasses memory-heavy alternative array trackers. The engine reads binary tracks, passes them through a clean `snprintf` macro loop, and pushes a standard 16-byte hex layout table row directly into current active string buffers.
* **Dual-Endianness Switching Architecture:** Implements a localized display formatting modifier flag (`hexl-toggle-endian`). This dynamically flips structural 16-bit layout byte pairings on screen between Big-Endian and Little-Endian visual modes on the fly without altering the raw underlying buffer tracking arrays.
* **Reverse Code Reconstruction:** When writing files back to storage, a synchronized scanning function strings text characters back into real hex offsets via `sscanf`, ensuring clean code writes while protecting our **447 KB static footprint**.

Reference code:
zzdwm/tools/utils/fileview/fileview.c
zzdwm/tools/utils/liveview/liveview.c
