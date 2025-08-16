#pragma once
#pragma message ( "Warning, not linux, spi is being stubbed" )

#include <stdint.h>

 #define SPI_CPHA                0x01
 #define SPI_CPOL                0x02
 
 #define SPI_MODE_0              (0|0)
 #define SPI_MODE_1              (0|SPI_CPHA)
 #define SPI_MODE_2              (SPI_CPOL|0)
 #define SPI_MODE_3              (SPI_CPOL|SPI_CPHA)
 
 #define SPI_CS_HIGH             0x04
 #define SPI_LSB_FIRST           0x08
 #define SPI_3WIRE               0x10
 #define SPI_LOOP                0x20
 #define SPI_NO_CS               0x40
 #define SPI_READY               0x80

#define SPI_IOC_WR_MODE 0
#define SPI_IOC_RD_MODE 1
#define SPI_IOC_WR_BITS_PER_WORD 2
#define SPI_IOC_RD_BITS_PER_WORD 3
#define SPI_IOC_WR_MAX_SPEED_HZ 4
#define SPI_IOC_RD_MAX_SPEED_HZ 5

struct spi_ioc_transfer {
    uint64_t tx_buf;
    uint64_t rx_buf;
    uint32_t len;
    uint32_t speed_hz;
    uint16_t delay_usecs;
    uint8_t  bits_per_word;
    uint8_t  cs_change;
    uint8_t  tx_nbits;
    uint8_t  rx_nbits;
    uint16_t pad;
 };

 #define SPI_IOC_MESSAGE(x) 0
 #define ioctl(x, y, z) -1

// Stub BCM2835 constants
#define BCM2835_SPI_BIT_ORDER_LSBFIRST 0
#define BCM2835_SPI_BIT_ORDER_MSBFIRST 1
#define BCM2835_SPI_MODE0 0
#define BCM2835_SPI_MODE1 1
#define BCM2835_SPI_MODE2 2
#define BCM2835_SPI_MODE3 3
#define BCM2835_SPI_CS0 0
#define BCM2835_SPI_CS1 1
#define BCM2835_GPIO_FSEL_OUTP 1
#define BCM2835_GPIO_FSEL_INPT 0
#define BCM2835_GPIO_PUD_UP 1
#define LOW 0

#define HIGH 1
static inline void bcm2835_gpio_write(int pin, int value) { (void)pin; (void)value; }
static inline int bcm2835_gpio_lev(int pin) { (void)pin; return 0; }
static inline void bcm2835_gpio_clr(int pin) { (void)pin; }
static inline void bcm2835_gpio_set(int pin) { (void)pin; }
static inline unsigned char bcm2835_spi_transfer(unsigned char value) { (void)value; return 0; }

// Stub BCM2835 functions
static inline int bcm2835_init() { return 1; }
static inline int bcm2835_spi_begin() { return 1; }
static inline void bcm2835_spi_setBitOrder(int order) { (void)order; }
static inline void bcm2835_spi_setDataMode(int mode) { (void)mode; }
static inline void bcm2835_spi_set_speed_hz(unsigned int hz) { (void)hz; }
static inline void bcm2835_spi_chipSelect(int cs) { (void)cs; }
static inline void bcm2835_spi_setChipSelectPolarity(int cs, int active) { (void)cs; (void)active; }
static inline void bcm2835_gpio_fsel(int pin, int mode) { (void)pin; (void)mode; }
static inline void bcm2835_gpio_set_pud(int pin, int pud) { (void)pin; (void)pud; }