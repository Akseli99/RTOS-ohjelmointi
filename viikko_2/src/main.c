#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

#define STACKSIZE 1024
#define PRIORITY 7

#define RED_NODE    DT_ALIAS(led0)
#define GREEN_NODE  DT_ALIAS(led1)
#define BLUE_NODE   DT_ALIAS(led2)

static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(RED_NODE, gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(GREEN_NODE, gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(BLUE_NODE, gpios);

volatile int led_state = 0; //tilamuuttuja, 0 = punainen, 1 = keltainen ja 2 = vihreä

// Funktioiden prototyypit
void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);

K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);

int init_leds(void) {
    if (!gpio_is_ready_dt(&red) || !gpio_is_ready_dt(&green) || !gpio_is_ready_dt(&blue)) {
        printk("LED init error");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);

    printk("LED init complete!");
    return 0;
}

void red_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 0) {
            gpio_pin_set_dt(&red, 1);
            k_msleep(1000);
            gpio_pin_set_dt(&red, 0);
            led_state = 1; 
        }
        k_msleep(100);
    }
}

void yellow_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 1) {
            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            k_msleep(1000);
            gpio_pin_set_dt(&red, 0);
            gpio_pin_set_dt(&green, 0);
            led_state = 2;
        }
        k_msleep(100);
    }
}

void green_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 2) {
            gpio_pin_set_dt(&green, 1);
            k_msleep(1000);
            gpio_pin_set_dt(&green, 0);
            led_state = 0; 
        }
        k_msleep(100);
    }
}

int main(void)
{
    init_leds();
    return 0;
}