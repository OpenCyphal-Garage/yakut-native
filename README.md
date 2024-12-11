# OpenCyphal Vehicle System Management Daemon

### Build

```
cd ocvsmd
mkdir build && cd build
cmake ..
make ocvsmd
```

### Installing

#### Installing the Daemon Binary:
```
sudo cp bin/ocvsmd /usr/local/bin/ocvsmd
```

#### Installing the Init Script:
```
sudo cp ../init.d/ocvsmd /etc/init.d/ocvsmd
sudo chmod +x /etc/init.d/ocvsmd
```

#### Enabling at Startup (on SysV-based systems):
```
sudo update-rc.d ocvsmd defaults
```

### Usage
```
sudo /etc/init.d/ocvsmd start
sudo /etc/init.d/ocvsmd status
sudo /etc/init.d/ocvsmd stop
```
