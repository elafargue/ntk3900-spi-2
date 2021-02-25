#include "spi_driver.h"
#ifdef __linux__
  #include <sys/ioctl.h>
  #include <linux/spi/spidev.h>
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
            InstanceMethod("mode", &SPIDriver::mode),
            InstanceMethod("chipSelect", &SPIDriver::chipSelect),
            InstanceMethod("bitsPerWord", &SPIDriver::bitsPerWord),
            InstanceMethod("maxSpeed", &SPIDriver::maxSpeed),
            InstanceMethod("wrPin", &SPIDriver::wrPin),
            InstanceMethod("rdyPin", &SPIDriver::rdyPin),
            InstanceMethod("invertRdy", &SPIDriver::invertRdy),
            InstanceMethod("bSeries", &SPIDriver::bSeries),
            InstanceMethod("delay", &SPIDriver::delay),
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
    m_bseries(false),
    m_invert_rdy(false)  // RDY is RDY, not BUSY
    {

}

SPIDriver::~SPIDriver(void) {

}

// Open expects a device passed as part of the info
Napi::Value SPIDriver::open(const Napi::CallbackInfo& info) {
    int retval = 0;

    if (!info[0].IsString()) {
        EXCEPTION("Wrong arguments - gabuzo")
    }
    ASSERT_NOT_OPEN;

    std::string dev = info[0].As<Napi::String>().Utf8Value();
    const char * device = dev.c_str();
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
        NULL,             //Any adddress in our space will do
        BLOCK_SIZE,       //Map length
        PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
        MAP_SHARED,       //Shared with other processes
        mem_fd,           //File to map
        GPIO_BASE         //Offset to GPIO peripheral
    );
    
    ::close(mem_fd); //No need to keep mem_fd open after mmap
    
    if (gpio_map == MAP_FAILED) {
        EXCEPTION("mmap error"); //errno also set!
    }

    printf("Ready pin: %u", this->m_rdy_pin);
    
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

    return info.This();

}

Napi::Value SPIDriver::close(const Napi::CallbackInfo& info) {
    ::close(this->m_fd);
    this->m_fd = -1;

    return info.This();
}

/**
 * The most important entry point, that receives the data to transfer.
 * Arg 1 and 2 are Buffers (and supposed to be the same size)
 */
Napi::Value SPIDriver::transfer(const Napi::CallbackInfo& info) {
    ASSERT_OPEN;

    if (info.Length() != 2 )
        EXCEPTION("Need two Buffer arguments");

    if (info[0].IsNull() && info[1].IsNull())
        EXCEPTION("Both buffers cannot be null");

    unsigned char *write_buffer = NULL;
    unsigned char *read_buffer = NULL;
    size_t write_length = -1;
    size_t read_length = -1;

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

    this->full_duplex_transfer(info, write_buffer, read_buffer,
                                 MAX(write_length, read_length),
                                 this->m_max_speed, this->m_delay, this->m_bits_per_word);

    return info.This();
}

void SPIDriver::full_duplex_transfer(
                const Napi::CallbackInfo& info,
                unsigned char *write,
                unsigned char *read,
                size_t length,
                uint32_t speed,
                uint16_t delay,
                uint8_t bits ) {

    struct spi_ioc_transfer data = {
	    (unsigned long)write,
        (unsigned long)read,
        1,
        speed,
        delay,
        bits,
        0,   // cs_change
        0,   // tx_nbits
        0,   // rx_nbits
        0    // pad
    };

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
        ret = ioctl(this->m_fd, SPI_IOC_MESSAGE(1), &data);

        if (this->m_wr_pin) {
            GPIO_CLR = 1 << this->m_wr_pin;
            GPIO_SET = 1 << this->m_wr_pin;
        }

        if (this->m_invert_rdy) {
            //For Series 7000 displays, the busy pin (spec says 20us max!)
            // can take a while to go up, so we have to add this delay. 10us
            // works well in practice.
            delayMicrosecondsHard(10); 
            while(GET_GPIO(this->m_rdy_pin)){};
        } else {
            // The RDY line can take up to 500ns to do down,
            // so we need to wait before reading it:
            delayMicrosecondsHard(1); 
            while(!GET_GPIO(this->m_rdy_pin)){};
        }
        //delayMicrosecondsHard(15);
    
        data.tx_buf++;
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
		return Napi::Number::New(info.Env(), this->m_mode);
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

Napi::Value SPIDriver::delay(const Napi::CallbackInfo& info) {
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
