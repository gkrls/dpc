## Environment variables


| Environment Variable            | Purpose                         | Allowed Values                       | Default |
| --------------------------------| ------------------------------- | ------------------------------------ | ------- |
| `DPC_LOG`/`DPC_LOG_LEVEL`       | Selects the logging level.      | `info`, `warn`, `debug`              | `info`  |
| `DPC_SCHEDULER`                 | Controls the context scheduler. | `on`, `off`, `fifo`, `fifo-threaded` | `off`   |
| `DPC_TIMEOUT`                   | Panic if collective not finished within that amount of ms | int >= `0` | `30000` |
