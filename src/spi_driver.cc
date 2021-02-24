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
    napi_value obj, value;

    Napi::Function func = DefineClass(
        env,
        "Spi",
        {
            InstanceMethod("open", &SPIDriver::open),
            InstanceMethod("close", &SPIDriver::close),
            InstanceMethod("transfer", &SPIDriver::transfer)
        }
    );

    // Export some of our constants to make sure the Javascript library uses values
    // that are consistent with the system values as defined in C headers
    NODE_SET_PROPERTY(exports, SPI_MODE_0)
    NODE_SET_PROPERTY(exports, SPI_MODE_1)
    NODE_SET_PROPERTY(exports, SPI_MODE_2)
    NODE_SET_PROPERTY(exports, SPI_MODE_3)

#define SPI_CS_LOW 0  // This doesn't exist normally
    NODE_SET_PROPERTY(exports,SPI_NO_CS)
    NODE_SET_PROPERTY(exports,SPI_CS_HIGH)
    NODE_SET_PROPERTY(exports,SPI_CS_LOW)

#define SPI_MSB false
#define SPI_LSB true
  NODE_SET_PROPERTY(exports, SPI_MSB);
  NODE_SET_PROPERTY(exports, SPI_LSB);


    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("SPIDriver", func);

    return exports;
}

SPIDriver::SPIDriver(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<SPIDriver>(info) {

}

SPIDriver::~SPIDriver(void) {

}

// Open expects a device passed as part of the info
Napi::Value SPIDriver::open(const Napi::CallbackInfo& info) {
    int retval = 0;

    if (!info[0].IsString()) {
        Napi::TypeError::New(info.Env(), "Wrong arguments").ThrowAsJavaScriptException();
        return info.Env().Null();
    }

    std::string dev = info[0].As<Napi::String>().Utf8Value();
    const char * device = dev.c_str();
    this->m_fd = ::open(device, O_RDWR); // Blocking!
    if (this->m_fd < 0) {
        EXCEPTION("Unable to open device");
        //return info.Env().Null();
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
        EXCEPTION("mmap error");//errno also set!
        
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


}

Napi::Value SPIDriver::close(const Napi::CallbackInfo& info) {
    
}

Napi::Value SPIDriver::transfer(const Napi::CallbackInfo& info) {
    
}