---
description: Flash firmware to the board
---

First run this command to find available USB ports:

```bash
ls /dev/cu.usbmodem* /dev/cu.SLAB* 2>/dev/null
```

Show the user the found ports as a selection via AskUserQuestion. Once the user has chosen a port, flash the firmware:

```bash
source ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py -p <selected-port> flash
```
