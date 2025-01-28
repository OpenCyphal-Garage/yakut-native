# OCVSMD - Open Cyphal Vehicle System Management Daemon

### Build
- Change directory to the project root; init submodules:
  ```
  cd ocvsmd
  git submodule update --init --recursive
  ```
Then one of the two presets depending on your system:
- `Demo-Linux` – Linux distros like Ubuntu.
- `Demo-BSD` – BSD based like MacOS.
  ###### Debug
  ```bash
  cmake --preset OCVSMD-Linux && \
  cmake --build --preset OCVSMD-Linux-Debug
  ```
  ###### Release
  ```bash
  cmake --preset OCVSMD-Linux && \
  cmake --build --preset OCVSMD-Linux-Release
  ```

### Installing

- Installing the Daemon Binary:
  ###### Debug
  ```bash
  sudo cp build/bin/Debug/ocvsmd /usr/local/bin/ocvsmd && \
  sudo cp build/bin/Debug/ocvsmd-cli /usr/local/bin/ocvsmd-cli
  ```
  ###### Release
  ```bash
  sudo cp build/bin/Release/ocvsmd /usr/local/bin/ocvsmd && \
  sudo cp build/bin/Release/ocvsmd-cli /usr/local/bin/ocvsmd-cli
  ```
- Installing the Init Script and Config file:
  ```bash
  sudo cp init.d/ocvsmd /etc/init.d/ocvsmd && \
  sudo chmod +x /etc/init.d/ocvsmd && \
  sudo mkdir -p /etc/ocvsmd && \
  sudo cp init.d/ocvsmd.toml /etc/ocvsmd/ocvsmd.toml
  ```

- Enabling at Startup if needed (on SysV-based systems):
  ```
  sudo update-rc.d ocvsmd defaults
  ```

### Usage
- Control the daemon using the following commands:
  ###### Start
  ```bash
  sudo /etc/init.d/ocvsmd start
  ```
  ###### Status
  ```bash
  sudo /etc/init.d/ocvsmd status
  ```
  ###### Restart
  ```bash
  sudo /etc/init.d/ocvsmd restart
  ```
  ###### Stop
  ```bash
  sudo /etc/init.d/ocvsmd stop
  ```
### Logging

#### View logs:
  - Syslog: 
    ```bash
    sudo grep -r "ocvsmd\[" /var/log/syslog
    ```
  - Log files:
    ```bash
    cat /var/log/ocvsmd.log
    ```
#### Manipulate log levels:

  Append `SPDLOG_LEVEL` and/or `SPDLOG_FLUSH_LEVEL` to the daemon command in the init script
  to enable more verbose logging level (`trace` or `debug`).
  Default level is `info`. More severe levels are: `warn`, `error` and `critical`.
  `off` level disables the logging.
  
By default, the log files are not immediately flushed to disk (at `off` level).
To enable flushing, set `SPDLOG_FLUSH_LEVEL` to a required default (or per component) level.

  - Example to set default level:
      ```bash
      sudo /etc/init.d/ocvsmd start SPDLOG_LEVEL=trace SPDLOG_FLUSH_LEVEL=trace
      ```
  - Example to set default and per component level (comma separated pairs):
      ```bash
      sudo /etc/init.d/ocvsmd restart SPDLOG_LEVEL=debug,ipc=off SPDLOG_FLUSH_LEVEL=warn,engine=debug
      ```
