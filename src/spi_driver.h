#pragma once

#include <napi.h>

// using namespace spi_driver;

#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

class SPIDriver : public Napi::ObjectWrap<SPIDriver> {
    public:
        SPIDriver(const Napi::CallbackInfo& info);
        static Napi::Object Init(Napi::Env env, Napi::Object exports);
        ~SPIDriver();

        Napi::Value open(const Napi::CallbackInfo& info);
        Napi::Value close(const Napi::CallbackInfo& info);
        Napi::Value transfer(const Napi::CallbackInfo& info);
        Napi::Value mode(const Napi::CallbackInfo& info);
        Napi::Value chipSelect(const Napi::CallbackInfo& info);
        Napi::Value bitsPerWord(const Napi::CallbackInfo& info);
        Napi::Value maxSpeed(const Napi::CallbackInfo& info);
        Napi::Value wrPin(const Napi::CallbackInfo& info);
        Napi::Value rdyPin(const Napi::CallbackInfo& info);
        Napi::Value invertRdy(const Napi::CallbackInfo& info);
        Napi::Value bSeries(const Napi::CallbackInfo& info);
        Napi::Value delay(const Napi::CallbackInfo& info);
        Napi::Value loopback(const Napi::CallbackInfo& info);
        Napi::Value bitOrder(const Napi::CallbackInfo& info);
        Napi::Value halfDuplex(const Napi::CallbackInfo& info);

    private:
        static Napi::FunctionReference constructor;
        void full_duplex_transfer(const Napi::CallbackInfo& info, unsigned char *write, unsigned char *read, size_t length, uint32_t speed, uint16_t delay, uint8_t bits);

        int m_fd;
        uint32_t m_mode;
        uint32_t m_max_speed;
        uint16_t m_delay;
        uint8_t m_bits_per_word;
        uint32_t m_wr_pin;
        uint32_t m_rdy_pin;
        bool m_bseries;
        bool m_invert_rdy;

};

#define EXCEPTION(MESSAGE) Napi::TypeError::New(info.Env(), #MESSAGE).ThrowAsJavaScriptException();

#define ASSERT_OPEN if (this->m_fd == -1) { EXCEPTION("Device not opened"); } 
#define ASSERT_NOT_OPEN if (this->m_fd != -1) { EXCEPTION("Cannot be called once device is opened"); }


// Macro to define a named property via n-api
#define NODE_SET_PROPERTY(exports, PROP) status = napi_create_int32(env, PROP, &value); \
    if (status != napi_ok) return exports; \
    status = napi_set_named_property(env, exports, #PROP, value);

// #define NODE_SET_PROPERTY(PROP)  status = napi_create_object(env, &obj); \
//     if (status != napi_ok) return exports; \
//     \
//     status = napi_create_int32(env, PROP, &value); \
//     if (status != napi_ok) return exports; \
//     status = napi_set_named_property(env, obj, #PROP, value);


#define SET_IOCTL_VALUE(FD, CTRL, VALUE)                                       \
  retval = ioctl(FD, CTRL, &(VALUE));                                          \
  if (retval == -1) {                                                          \
    EXCEPTION("Unable to set " #CTRL);                                         \
  }

#define MAX(a,b) (a>b ? a:b)
