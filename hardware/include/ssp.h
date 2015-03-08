#pragma once

#include <decl.h>
#include <stdint.h>
#include <gpio.h>

// ought to be generic, cm3-specific for now
typedef enum {
    ssp_port_0,
    ssp_port_1,
} ssp_t;

typedef uint64_t ssp_port_t;

#define MAKE_SSP_PORT(ssp, sclk, mosi, miso) (\
(((uint64_t) ssp) << 48) | \
(((uint64_t) sclk) << 32) | \
(((uint64_t) mosi) << 16) | \
(((uint64_t) miso)))

BEGIN_DECL

static inline ssp_t get_ssp(ssp_port_t ssp_port)  { return (ssp_port >> 48) & 0xffff; }
static inline pin_t get_sclk(ssp_port_t ssp_port) { return (ssp_port >> 32) & 0xffff; }
static inline pin_t get_mosi(ssp_port_t ssp_port) { return (ssp_port >> 16) & 0xffff; }
static inline pin_t get_miso(ssp_port_t ssp_port) { return (ssp_port)       & 0xffff; }

int ssp_config(ssp_port_t ssp_port, uint32_t clock);
uint8_t ssp_readwrite(ssp_t ssp, uint8_t value);
static inline void ssp_write(ssp_t ssp, uint8_t value) { (void) ssp_readwrite(ssp, value); }
static inline uint8_t ssp_read(ssp_t ssp) { return ssp_readwrite(ssp, 0xff); }

END_DECL
