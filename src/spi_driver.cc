#include "spi_driver.h"
#ifdef __linux__
  #include <sys/ioctl.h>
  #include <linux/spi/spidev.h>
  #include "bcm2835.h"
#else
  #include "fake_spi.h"
#endif


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>


// I/O access
volatile unsigned *gpio;
void *gpio_map;

// using namespace spi_driver;

void delayMicrosecondsHard (unsigned int howLong)
 {
   struct timeval tNow, tLong, tEnd ;
 
   gettimeofday (&tNow, NULL) ;
   tLong.tv_sec  = howLong / 1000000 ;
   tLong.tv_usec = howLong % 1000000 ;
   timeradd (&tNow, &tLong, &tEnd) ;
 
   while (timercmp (&tNow, &tEnd, <))
     gettimeofday (&tNow, NULL) ;
 }


Napi::FunctionReference SPIDriver::constructor;

Napi::Object SPIDriver::Init(Napi::Env env, Napi::Object exports) {
    napi_status status;
    napi_value value;

    Napi::Function func = DefineClass(
        env,
        "Spi",
        {
            InstanceMethod("open", &SPIDriver::open),
            InstanceMethod("close", &SPIDriver::close),
            InstanceMethod("transfer", &SPIDriver::transfer),
            InstanceMethod("dmaTransfer", &SPIDriver::dmaTransfer),
            InstanceMethod("driver", &SPIDriver::driver),
            InstanceMethod("mode", &SPIDriver::mode),
            InstanceMethod("chipSelect", &SPIDriver::chipSelect),
            InstanceMethod("bitsPerWord", &SPIDriver::bitsPerWord),
            InstanceMethod("maxSpeed", &SPIDriver::maxSpeed),
            InstanceMethod("wrPin", &SPIDriver::wrPin),
            InstanceMethod("rdyPin", &SPIDriver::rdyPin),
            InstanceMethod("invertRdy", &SPIDriver::invertRdy),
            InstanceMethod("bSeries", &SPIDriver::bSeries),
            InstanceMethod("delay", &SPIDriver::spiDelay),
            InstanceMethod("loopback", &SPIDriver::loopback),
            InstanceMethod("bitOrder", &SPIDriver::bitOrder),
            InstanceMethod("halfDuplex", &SPIDriver::halfDuplex),
        }
    );

    // Export some of our constants to make sure the Javascript library uses values
    // that are consistent with the system values as defined in C headers
    NODE_SET_PROPERTY(exports, SPI_MODE_0)
    NODE_SET_PROPERTY(exports, SPI_MODE_1)
    NODE_SET_PROPERTY(exports, SPI_MODE_2)
    NODE_SET_PROPERTY(exports, SPI_MODE_3)

    // This module supports either spidev - good but slow, or very very slow for
    // short transfers, and low level bcm2835 library, which is very fast and
    // a bit more dangerous since it bypasses the Linux bcm-2835 driver and talks
    // to the chip directly.
    NODE_SET_PROPERTY(exports, DRIVER_SPIDEV)
    NODE_SET_PROPERTY(exports, DRIVER_BCM2835)

#define SPI_CS_LOW 0  // This doesn't exist normally
    NODE_SET_PROPERTY(exports, SPI_NO_CS)
    NODE_SET_PROPERTY(exports, SPI_CS_HIGH)
    NODE_SET_PROPERTY(exports, SPI_CS_LOW)

#define SPI_MSB false
#define SPI_LSB true
    NODE_SET_PROPERTY(exports, SPI_MSB);
    NODE_SET_PROPERTY(exports, SPI_LSB);

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("Spi", func);

    return exports;
}

SPIDriver::SPIDriver(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<SPIDriver>(info),
    m_fd(-1),
    m_mode(0),
    m_max_speed(1000000),  // default speed in Hz () 1MHz
    m_delay(0),            // expose delay to options
    m_bits_per_word(8),    // default bits per word
    m_wr_pin(0),
    m_rdy_pin(0),
    m_driver(DRIVER_SPIDEV),
    m_bseries(false),
    m_invert_rdy(false)  // RDY is RDY, not BUSY
    {

}

SPIDriver::~SPIDriver(void) {

}

// Open expects a device passed as part of the info
Napi::Value SPIDriver::open(const Napi::CallbackInfo& info) {

    if (!info[0].IsString()) {
        EXCEPTION("Wrong arguments")
    }
    ASSERT_NOT_OPEN;

    std::string dev = info[0].As<Napi::String>().Utf8Value();
    const char * device = dev.c_str();

    if (this->m_driver == DRIVER_SPIDEV) {
        open_spidev(info, device);
    } else {
        open_bcm2835(info, device);
    }

    return info.This();

}

Napi::Value SPIDriver::close(const Napi::CallbackInfo& info) {
    ::close(this->m_fd);
    this->m_fd = -1;

    return info.This();
}

/**
 * Standard SPI transfer entry point, that receives the data to transfer.
 * Arg 1 and 2 are Buffers (and supposed to be the same size).
 */
Napi::Value SPIDriver::transfer(const Napi::CallbackInfo& info) {
    ASSERT_OPEN;

    this->do_transfer(info, false);

    return info.This();
}

Napi::Value SPIDriver::dmaTransfer(const Napi::CallbackInfo& info) {
    ASSERT_OPEN;

    this->do_transfer(info, true);

    return info.This();
}


/**
 * Executes the transfer in dma or standard mode
 */
void SPIDriver::do_transfer(const Napi::CallbackInfo& info, bool dma) {
    ASSERT_OPEN;

    if (!(info.Length() >= 1) )
        EXCEPTION("Need at least one Buffer argument");

    if (info[0].IsNull() && info[1].IsNull())
        EXCEPTION("Both buffers cannot be null");

    unsigned char *write_buffer = NULL;
    unsigned char *read_buffer = NULL;
    size_t write_length = 0;
    size_t read_length = 0;

    // Setup the pointer to the data in both buffers
    if (info[0].IsBuffer()) {
         Napi::Buffer<uint8_t> write_buffer_obj = info[0].As<Napi::Buffer<uint8_t>>();
         write_length = write_buffer_obj.Length();
         write_buffer = write_buffer_obj.Data();
    }
    
    if (info[1].IsBuffer()) {
         Napi::Buffer<uint8_t> read_buffer_obj = info[1].As<Napi::Buffer<uint8_t>>();
         read_length = read_buffer_obj.Length();
         read_buffer = read_buffer_obj.Data();
    }

    if (write_length > 0 && read_length > 0 && write_length != read_length) {
         EXCEPTION("Read and write buffers MUST be the same length");
    }

    if (this->m_driver == DRIVER_SPIDEV) {
        this->spidev_transfer(info, write_buffer, read_buffer,
                                    MAX(write_length, read_length),
                                    this->m_max_speed, this->m_delay, this->m_bits_per_word, dma);
    } else {
        this->bcm2835_transfer(info, write_buffer, read_buffer,
                                    MAX(write_length, read_length),
                                    this->m_max_speed, this->m_delay, this->m_bits_per_word, dma);
    }
}

/**
 * Opens a SPI peripheral using the Linux spidev interface (/dev/spiX.Y) 
 */
void SPIDriver::open_spidev(const Napi::CallbackInfo& info, const char * device) {
    int retval = 0;

    this->m_fd = ::open(device, O_RDWR); // Blocking!
    if (this->m_fd < 0) {
        EXCEPTION("Unable to open device");
    }

    SET_IOCTL_VALUE(this->m_fd, SPI_IOC_WR_MODE, this->m_mode);
    SET_IOCTL_VALUE(this->m_fd, SPI_IOC_WR_BITS_PER_WORD, this->m_bits_per_word);
    SET_IOCTL_VALUE(this->m_fd, SPI_IOC_WR_MAX_SPEED_HZ, this->m_max_speed);

    // Setup the GPIO pin as well
    int mem_fd;
    /* open /dev/mem */
    if ((mem_fd = ::open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        EXCEPTION("can't open /dev/mem");
    }
    
    /* mmap GPIO */
    gpio_map = mmap(
        NULL,                 //Any adddress in our space will do
        BLOCK_SIZE,           //Map length
        PROT_READ|PROT_WRITE, // Enable reading & writing to mapped memory
        MAP_SHARED,           //Shared with other processes
        mem_fd,               //File to map
        GPIO_BASE             //Offset to GPIO peripheral
    );
    
    ::close(mem_fd); //No need to keep mem_fd open after mmap
    
    if (gpio_map == MAP_FAILED) {
        EXCEPTION("mmap error"); //errno also set!
    }
    
    // Always use volatile pointer!
    gpio = (volatile unsigned *)gpio_map;

    INP_GPIO(this->m_wr_pin);
    OUT_GPIO(this->m_wr_pin);

    INP_GPIO(this->m_rdy_pin);
    // Enable pulldown on Ready pin:
    GPIO_PULL = 1;
    delayMicrosecondsHard(5);
    GPIO_PULLCLK0 = 1 << this->m_rdy_pin;
    delayMicrosecondsHard(5);
    GPIO_PULL = 0;
    GPIO_PULLCLK0 = 0;

}

/**
 * Opens a SPI peripheral using the bcm2835 direct access library.
 * Use "/dev/spidev0.0" for SPI0 and CS0, and anything else for CS1. It does not
 * really use /dev/spidev but we keep the syntax for compatibility.
 */
void SPIDriver::open_bcm2835(const Napi::CallbackInfo& info, const char * device) {
    if (!bcm2835_init())
        EXCEPTION("bcm2835_init failed. Are you running as root?");

    if (!bcm2835_spi_begin())
        EXCEPTION("bcm2836_spi_begin failed.");

    // Configure the SPI bus (SPI0 only for now) using our settings.
    // AFAIK there's no direct mapping between Linux SPIDEV defines
    // which are used for our settings, and the bcm2835 library, so we
    // are doing dumb mappings here:
    if (this->m_mode & SPI_LSB_FIRST) {
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_LSBFIRST);
    } else {
        bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    }

    switch (this->m_mode & 0x03) {
        case SPI_MODE_0:
            bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
        break;
        case SPI_MODE_1:
            bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);
        break;
        case SPI_MODE_2:
            bcm2835_spi_setDataMode(BCM2835_SPI_MODE2);
        break;
        case SPI_MODE_3:
            bcm2835_spi_setDataMode(BCM2835_SPI_MODE3);
        break;

    }

    bcm2835_spi_set_speed_hz(this->m_max_speed);
    if (! strcmp(device, "/dev/spidev0.0")) {
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
        this->m_fd = 0; // We use m_fd to remember what CS line is used
    } else {
        bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
        bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS1, LOW);
        this->m_fd = 1;
    }

    // Now setup the GPIOs that are specific to the Noritake screens

    if (this->m_wr_pin)
        bcm2835_gpio_fsel(this->m_wr_pin, BCM2835_GPIO_FSEL_OUTP);

    if (this->m_rdy_pin) {
        bcm2835_gpio_fsel(this->m_rdy_pin, BCM2835_GPIO_FSEL_INPT);
        bcm2835_gpio_set_pud(this->m_rdy_pin, BCM2835_GPIO_PUD_UP);
    }


}

/**
 * The core of SPI transfers - spidev version
 */
void SPIDriver::spidev_transfer(
                const Napi::CallbackInfo& info,
                unsigned char *tx_buf,
                unsigned char *rx_buf,
                size_t length,
                uint32_t speed,
                uint16_t delay,
                uint8_t bits,
                bool dma ) {

    int ret =0;
    GPIO_SET = 1 << this->m_wr_pin;

    // Don't write anything if the peripheral is not ready
    if (this->m_invert_rdy) {
        while(GET_GPIO(this->m_rdy_pin)){};
    } else {
        while(!GET_GPIO(this->m_rdy_pin)){};
    }

    // Now send byte by byte for the whole buffer
    while (length--) {
        // struct timeval tNow,tEnd ;
        // gettimeofday (&tNow, NULL);
        if (this->m_wr_pin) {
            GPIO_CLR = 1 << this->m_wr_pin;
        }
        ret = write(this->m_fd, tx_buf++, 1);
        // gettimeofday (&tEnd, NULL);
        // printf("Took %lu\n", tEnd.tv_usec-tNow.tv_usec);

        if (this->m_wr_pin) {
            GPIO_SET = 1 << this->m_wr_pin;
        }

        if (this->m_invert_rdy) {
            //For Series 7000 displays, the busy pin (spec says 20us max!)
            // can take a while to go up, so we have to add this delay. 10us
            // works well in practice.
            delayMicrosecondsHard(10); 
            while(GET_GPIO(this->m_rdy_pin)){};
        } else if (!dma) {
            // The RDY line can take up to 500ns to do down,
            // so we need to wait before reading it:
            delayMicrosecondsHard(1); 
            while(!GET_GPIO(this->m_rdy_pin)){};
        }
    
        //data.tx_buf++;
    }

  if (ret == -1) {
    EXCEPTION("Unable to send SPI message");
  }

}

/**
 * The core of SPI transfers - BCM2835 version
 */
void SPIDriver::bcm2835_transfer(
                const Napi::CallbackInfo& info,
                unsigned char *tx_buf,
                unsigned char *rx_buf,
                size_t length,
                uint32_t speed,
                uint16_t delay,
                uint8_t bits,
                bool dma ) {

    int ret =0;

    // Since we can have multiple instances of SPI, we have to reset
    // the peripheral before each transfer since they can all have different
    // speed values and CS line selection
    bcm2835_spi_set_speed_hz(speed);
    if (this->m_fd) {
        bcm2835_spi_chipSelect(BCM2835_SPI_CS1);
    } else {
        bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    }

    if (this->m_wr_pin)
        bcm2835_gpio_write(this->m_wr_pin,HIGH);

    // Don't write anything if the peripheral is not ready
    if (this->m_invert_rdy) {
        while(bcm2835_gpio_lev(this->m_rdy_pin)) {};
    } else {
        while(! bcm2835_gpio_lev(this->m_rdy_pin)) {};
    }

    // Now send byte by byte for the whole buffer and check
    // the busy/ready signal at each byte if necessary, and also
    // toggle the !WRITE signal if necessary
    while (length--) {
        if (this->m_wr_pin) {
            bcm2835_gpio_clr(this->m_wr_pin);
        }
        ret = bcm2835_spi_transfer(*tx_buf);
        tx_buf++;

        if (this->m_wr_pin) {
            bcm2835_gpio_set(this->m_wr_pin);
        }

        if (this->m_invert_rdy) {
            //For Series 7000 displays, the busy pin (spec says 20us max!)
            // can take a while to go up, so we have to add this delay. 10us
            // works well in practice.
            delayMicrosecondsHard(10); 
            while(bcm2835_gpio_lev(this->m_rdy_pin)) {};
        } else if (! dma ) {
            // The RDY line can take up to 500ns to do down,
            // so we need to wait before reading it:
            delayMicrosecondsHard(1); 
            while(!bcm2835_gpio_lev(this->m_rdy_pin)) {};
        }
    }

  if (ret == -1) {
    EXCEPTION("Unable to send SPI message");
  }

}

Napi::Value SPIDriver::mode(const Napi::CallbackInfo& info) {
	if (info.Length() > 0 && info[0].IsNumber()) {
		uint32_t in_mode = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        if (in_mode == SPI_MODE_0 || in_mode == SPI_MODE_1 ||
            in_mode == SPI_MODE_2 || in_mode == SPI_MODE_3) {
// TODO: Shouldn't we mask mode to remove the CS bits ?
            this->m_mode = in_mode;
        } else {
            EXCEPTION("Argument 1 must be one of the SPI_MODE_X constants");
        }
		return info.This();
	}
	else {
		return Napi::Number::New(info.Env(), this->m_mode & 0x03);
	}
}

/**
 * Sets the CS line
 */
Napi::Value SPIDriver::chipSelect(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        // Set
        uint32_t in_value = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        switch(in_value) {
        case SPI_CS_HIGH:
            this->m_mode |= SPI_CS_HIGH;
            this->m_mode &= ~SPI_NO_CS;
            break;
        case SPI_NO_CS:
            this->m_mode |= SPI_NO_CS;
            this->m_mode &= ~SPI_CS_HIGH;
            break;
        default:
            this->m_mode &= ~(SPI_NO_CS|SPI_CS_HIGH);
            break;
        }
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_mode & (SPI_CS_HIGH|SPI_NO_CS));
    }
}


Napi::Value SPIDriver::bitsPerWord(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        uint32_t in_value = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        // TODO: Bounds checking?  Need to look up what the max value is
        this->m_bits_per_word = in_value;
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_bits_per_word);
    }
}

Napi::Value SPIDriver::maxSpeed(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        uint32_t in_value = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        // TODO: Bounds checking?  Need to look up what the max value is
        this->m_max_speed = in_value;
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_max_speed);
    }
}

Napi::Value SPIDriver::wrPin(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        uint32_t in_value = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        // TODO: Bounds checking?  Need to look up what the max value is
        this->m_wr_pin = in_value;
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_wr_pin);
    }
}

/**
 * Specific to Noritake VFD screen support: those screens have a RDY
 * pin that has to be monitored, we can't just stuff data into them.
 *  This is especially important when driving a parallel screen through
 *  a SPI shift register like the 74HC595.
 * 
 * In the words of Noritake (3900B series, parallel mode):
 * Internal receive buffer capacity is 256 bytes. After data is input,
 * RDY signal is immediately set to RDY=0 (BUSY) until the received byte
 * is stored to the receive buffer. If the internal receive buffer is full,
 * RDY signal will remain BUSY until space for 1 byte becomes available.
 * The required time for this varies, depending on the type of commands
 * and rate at which data is input. The RDY signal should always be checked
 * before writing data.
 */
Napi::Value SPIDriver::rdyPin(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        uint32_t in_value = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        // TODO: Bounds checking?  Need to look up what the max value is
        this->m_rdy_pin = in_value;
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_rdy_pin);
    }
}

/**
 * Specific to Noritake again - because some RDY are active when up,
 * and some active when down
 */
Napi::Value SPIDriver::invertRdy(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsBoolean()) {
        bool in_value = info[0].As<Napi::Boolean>().Value();
        ASSERT_NOT_OPEN;
        this->m_invert_rdy = in_value;
        return info.This();
    } else {
        return Napi::Boolean::New(info.Env(), this->m_invert_rdy);
    }
}

/**
 * Specific to Noritake VFD screen support
 */
Napi::Value SPIDriver::bSeries(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsBoolean()) {
        bool in_value = info[0].As<Napi::Boolean>().Value();
        ASSERT_NOT_OPEN;
        this->m_bseries = in_value;
        return info.This();
    } else {
        return Napi::Boolean::New(info.Env(), this->m_bseries);
    }
}

Napi::Value SPIDriver::spiDelay(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        uint16_t in_value = info[0].As<Napi::Number>().DoubleValue();
        ASSERT_NOT_OPEN;
        // TODO: Bounds checking?  Need to look up what the max value is
        this->m_delay = in_value;
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_delay);
    }
}

Napi::Value SPIDriver::loopback(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsBoolean()) {
        bool in_value = info[0].As<Napi::Boolean>().Value();
        ASSERT_NOT_OPEN;
        if (in_value) {
            this->m_mode |= SPI_LOOP;
        } else {
            this->m_mode &= ~SPI_LOOP;
        }
        return info.This();
    } else {
        return Napi::Boolean::New(info.Env(), (this->m_mode & SPI_LOOP) > 0);
    }
}

/**
 * bitOrder is set to true for LSB first
 */
Napi::Value SPIDriver::bitOrder(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsBoolean()) {
        bool in_value = info[0].As<Napi::Boolean>().Value();
        ASSERT_NOT_OPEN;
        if (in_value) {
            this->m_mode |= SPI_LSB_FIRST;
        } else {
            this->m_mode &= ~SPI_LSB_FIRST;
        }
        return info.This();
    } else {
        return Napi::Boolean::New(info.Env(), (this->m_mode & SPI_LSB_FIRST) > 0);
    }
}

/**
 * halfDuplex is set to true for half duplex
 */
Napi::Value SPIDriver::halfDuplex(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsBoolean()) {
        bool in_value = info[0].As<Napi::Boolean>().Value();
        ASSERT_NOT_OPEN;
        if (in_value) {
            this->m_mode |= SPI_3WIRE;
        } else {
            this->m_mode &= ~SPI_3WIRE;
        }
        return info.This();
    } else {
        return Napi::Boolean::New(info.Env(), (this->m_mode & SPI_3WIRE) > 0);
    }
}

/**
 * driver picks whether we want spidev or low level bcm2835 support
 */
Napi::Value SPIDriver::driver(const Napi::CallbackInfo& info) {
    if (info.Length() > 0 && info[0].IsNumber()) {
        uint32_t in_value = info[0].As<Napi::Number>().Uint32Value();
        ASSERT_NOT_OPEN;
        if (in_value == DRIVER_SPIDEV) {
            this->m_driver = DRIVER_SPIDEV;
        } else {
            this->m_driver = DRIVER_BCM2835;
        }
        return info.This();
    } else {
        return Napi::Number::New(info.Env(), this->m_driver);
    }
}
