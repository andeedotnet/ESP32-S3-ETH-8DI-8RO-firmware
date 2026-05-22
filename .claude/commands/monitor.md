---
description: Open serial monitor (idf.py monitor)
---

First run this command to find available USB ports:

```bash
ls /dev/cu.usbmodem* /dev/cu.SLAB* 2>/dev/null
```

Show the user the found ports as a selection via AskUserQuestion. Once the user has chosen a port, start the monitor:

```bash
source ~/.espressif/v6.0.1/esp-idf/export.sh && idf.py -p <selected-port> monitor
```

Note to user: Exit monitor with **Ctrl+]**
