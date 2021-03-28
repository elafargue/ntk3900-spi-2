# NodeJS SPI module with extensions for VFD displays

This is a NodeJS module that uses the existing API from the old and now unmaintained Node SPI module, but
is ported to use the Node NAPI API that should make it a lot more maintainable.

It also supports two SPI modes: either Linux "spidev" userland API, that writes to the /dev/spidevX.Y devices, or
on Raspberry Pi only, you can open a SPI device using the "BCM2835" protocol that uses the low level bcm2835 library
for _vastly_ improved performance in case you need to write while toggling other GPIOs, like I need to do on the parallel
mode 3900 series VFDs through a SPI bus and a 74HC595 shift register.

Example:

```
    this.dev = new SPI.Spi('/dev/spidev0.' + options.bus, {
            'mode': SPI.MODE['MODE_3'],
            'chipSelect': SPI.CS['low'],
            'driver': SPI.DRIVER['BCM2835'],
            'wrPin': 23,
            'rdyPin': 22,
            'bSeries': options.bseries,
            'maxSpeed': 20e6  // Tested up to 25MHz (this is RPi -> 74HC595)
        }, function(s) { if (process.platform == 'linux') s.open(); }
    );
```

`wrPin` is a pin that will be toggled during each write (low / write byte / high). `rdyPin` will be monitored and will block until the screen is ready to write the next byte.

TODO: document the entire API. `lib/binding/js` is your friend in the mean time.

# License

Because this node module uses a copy of the bcm2835 library in its source tree (version bcm2835-1.68) from http://www.airspayce.com/mikem/bcm2835/index.html and that one is GPLv3, this code is also GPLv3.

If you don't like GPLv3, out of luck, since the library is GPLv3 and not LGPL, which means that even linking against it will put your code under the terms of GPLv3. It's a spare time project anyway, so it's a perfectly appropriate license.