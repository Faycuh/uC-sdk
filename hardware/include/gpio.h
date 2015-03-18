#pragma once

#include <decl.h>
#include <stdint.h>

BEGIN_DECL

#define GPIO_PORT_A 0
#define GPIO_PORT_B 1
#define GPIO_PORT_C 2
#define GPIO_PORT_D 3
#define GPIO_PORT_E 4
#define GPIO_PORT_F 5
#define GPIO_PORT_G 6
#define GPIO_PORT_H 7
#define GPIO_PORT_I 8
#define GPIO_PORT_J 9
#define GPIO_PORT_K 10

typedef struct {
    uint8_t port;
    uint8_t pin;
} pin_t;

_Static_assert(sizeof(pin_t) == 2, "pin_t isn't 16 bits-wide");

/*
// should be for CM3 - that's for everyone for now
typedef uint16_t pin_t;
#define MAKE_PIN(port, pin) ((pin_t)(((port & 0xff) << 8) | (pin & 0xff)))

static __inline__ uint8_t get_port(pin_t pin) { return (pin >> 8) & 0xff; }
static __inline__ uint8_t get_pin(pin_t pin) { return pin & 0xff; }
*/
typedef enum {
    pin_dir_read = 0,
    pin_dir_write = 1,
} pin_dir_t;

typedef enum {
    pull_none = 0,
    pull_up = 1,
    pull_down = 2,
} pull_t;

static __inline__ pin_t make_pin(uint8_t port, uint8_t pin) { pin_t p = { port, pin }; return p; }
void gpio_config(pin_t pin, pin_dir_t dir, pull_t pull);
void gpio_set(pin_t pin, int enabled);
uint8_t gpio_get(pin_t pin);

END_DECL
