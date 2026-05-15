## Configuration precedence:

| What                              | Source priority                  |
| ----------------------------------|----------------------------------|
| Backend selection                 | explicit > env > default         |
| Device config                     | explicit > env > default         |
| Field values within a config      | DPC_[BACKEND]_[FIELD] > JSON file > defaults |

"Explicit" means a value passed to the Context constructor.

## Environment variables


| Environment Variable            | Purpose                         | Allowed Values                       | Default |
| --------------------------------| ------------------------------- | ------------------------------------ | ------- |
| `DPC_LOG`/`DPC_LOG_LEVEL`       | Selects the logging level.      | `info`, `warn`, `debug`              | `info`  |
| `DPC_SCHEDULER`                 | Controls the context scheduler. | `on`, `off`, `fifo`, `fifo-threaded` | `off`   |
| `DPC_TIMEOUT`                   | Panic if collective not finished within that amount of ms | int >= `0` | `30000` |
| `DPC_IFACE`                     | Interface to use. Error if not found or no bound IPv4. If no `DPC_ADDR` pick first IPv4 or error |   | `""` (auto) |
| `DPC_ADDR`                      | IPv4 address to use. Must be bound to `DPC_IFACE` if set. Error if not found | | `""` (auto) |
