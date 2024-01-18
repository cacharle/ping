# ping

Rewrite of the ping command (based on the school 42 project ft\_ping).

## Usage

```
meson setup build
ninja -C build
./build/ping google.com
```

You have to be root to run the last command since I'm using raw sockets.
