# logOS ESP-IDF dependency

logOS uses an unmodified ESP-IDF component tree. ESP-IDF's default
`call_start_cpu0()` performs low-level chip initialization, then dispatches to
the strong `start_cpu0()` supplied by `main/start_cpu0.c`. logOS takes control
there, before FreeRTOS starts.

The currently tested base is:

- ESP-IDF commit: `fa8039b5ca`
- Described version: `v6.1-dev-5824-gfa8039b5ca`

Activate ESP-IDF before building:

```sh
. /path/to/esp-idf/export.sh
idf.py build
```

No project-level `components/esp_system` override or customized ESP-IDF fork
is required.
