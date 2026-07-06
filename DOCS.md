## Configuration precedence:

| What                              | Source priority                  |
| ----------------------------------|----------------------------------|
| Backend selection                 | explicit > env > default         |
| Device config                     | explicit > env > default         |
| Field values within a config      | DPC_[BACKEND]_[FIELD] > JSON file > defaults |

"Explicit" means a value passed to the Context constructor.

## Environment variables


| Environment Variable            | Purpose                         | Allowed Values                         | Default |
| --------------------------------| ------------------------------- | -------------------------------------- | ------- |
| `DPC_LOG`/`DPC_LOG_LEVEL`       | Selects the logging level.      | `info`, `warn`, `debug`                | `info`  |
| `DPC_CONFIG`                    | Path to worker_config.json      | ...                                    | `""`    |
| `DPC_DEVICE`                    | Path to device_config.json      | ...                                    | `""`    |
| `DPC_BACKEND`                   | Which backend to use            | `noop`, `sock`, `dpdk`                 | `sock`  |
| `DPC_SCHEDULER`                 | Controls the context scheduler. | `on`, `off`, `fifo`, `fifo-threaded`   | `off`   |
| `DPC_TIMEOUT`                   | Panic if collective not finished within that amount of ms | float >= `0` | `30`    |
| `DPC_IFACE`                     | Interface to use. Error if not found or w/o IPv4. If no `DPC_ADDR` pick first or error | -  | `""` (auto) |
| `DPC_ADDR`                      | IPv4 address to use. Must be bound to `DPC_IFACE` if set. Error if not found           | -  | `""` (auto) |
| `DPC_PORT`                      | UDP port to use                 | int >= `0` | `4242` (default) |
