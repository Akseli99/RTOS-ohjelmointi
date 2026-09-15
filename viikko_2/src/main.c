/*tässä tehtävässä tavoitteena oli 3 pistettä
-Pause toiminto on toteutettu ohjeen mukaan
-Valojen / keltaisen vilkkumisen ohjaus napeilla on myös toteutettu ohjeen mukaan
-Nappien pinnit on määritelty hieman toisin kuin tehtävänanossa, esimerkiksi pause-nappi on sw2 eikä sw0*/




#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>

#define STACKSIZE 1024
#define PRIORITY 7

#define RED_NODE    DT_ALIAS(led0)
#define GREEN_NODE  DT_ALIAS(led1)
#define BLUE_NODE   DT_ALIAS(led2)

#define BUTTON_4 DT_ALIAS(sw4) // vilkkuva keltainen valo
#define BUTTON_3 DT_ALIAS(sw3) // vihreä valo päälle
#define BUTTON_2 DT_ALIAS(sw1) // keltainen valo päälle
#define BUTTON_1 DT_ALIAS(sw0) // punainen valo päälle
#define BUTTON_0 DT_ALIAS(sw2) // pause toiminto

static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(RED_NODE, gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(GREEN_NODE, gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(BLUE_NODE, gpios);

static const struct gpio_dt_spec button_0_spec = GPIO_DT_SPEC_GET_OR(BUTTON_0, gpios, {0});
static const struct gpio_dt_spec button_1_spec = GPIO_DT_SPEC_GET_OR(BUTTON_1, gpios, {0});
static const struct gpio_dt_spec button_2_spec = GPIO_DT_SPEC_GET_OR(BUTTON_2, gpios, {0});
static const struct gpio_dt_spec button_3_spec = GPIO_DT_SPEC_GET_OR(BUTTON_3, gpios, {0});
static const struct gpio_dt_spec button_4_spec = GPIO_DT_SPEC_GET_OR(BUTTON_4, gpios, {0});

static struct gpio_callback button_0_cb_data;
static struct gpio_callback button_1_cb_data;
static struct gpio_callback button_2_cb_data;
static struct gpio_callback button_3_cb_data;
static struct gpio_callback button_4_cb_data;

volatile int led_state = 0; // 0 = punainen, 1 = keltainen, 2 = vihreä, 4 = pause, 5 = vilkkuminen
volatile int previous_state = 0;

// Funktioiden prototyypit
void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);
void blink_yellow_task(void *, void *, void *);

void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(blink_thread, STACKSIZE, blink_yellow_task, NULL, NULL, NULL, PRIORITY, 0, 0);

int init_leds(void) {
    if (!gpio_is_ready_dt(&red) || !gpio_is_ready_dt(&green) || !gpio_is_ready_dt(&blue)) {
        printk("LED init error\n");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&red, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&blue, GPIO_OUTPUT_INACTIVE);

    printk("LED init complete!\n");
    return 0;
}

void red_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 0) {
            gpio_pin_set_dt(&red, 1);
            int elapsed = 0;
            while (elapsed < 1000) {
                if (led_state == 4 || led_state == 5) {
                    k_msleep(100);
                    continue;
                }
                k_msleep(100);
                elapsed += 100;
            }
            if (led_state == 0) {
                gpio_pin_set_dt(&red, 0);
                led_state = 1; 
            }
        }
        k_msleep(100);
    }
}

void yellow_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 1) {
            gpio_pin_set_dt(&red, 1);
            gpio_pin_set_dt(&green, 1);
            int elapsed = 0;
            while (elapsed < 1000) {
                if (led_state == 4 || led_state == 5) {
                    k_msleep(100);
                    continue;
                }
                k_msleep(100);
                elapsed += 100;
            }
            if (led_state == 1) {
                gpio_pin_set_dt(&red, 0);
                gpio_pin_set_dt(&green, 0);
                led_state = 2;
            }
        }
        k_msleep(100);
    }
}

void green_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 2) {
            gpio_pin_set_dt(&green, 1);
            int elapsed = 0;
            while (elapsed < 1000) {
                if (led_state == 4 || led_state == 5) {
                    k_msleep(100);
                    continue;
                }
                k_msleep(100);
                elapsed += 100;
            }
            if (led_state == 2) {
                gpio_pin_set_dt(&green, 0);
                led_state = 0; 
            }
        }
        k_msleep(100);
    }
}

void blink_yellow_task(void *p1, void *p2, void *p3) {
    while (true) {
        if (led_state == 5) {
            gpio_pin_toggle_dt(&red);
            gpio_pin_toggle_dt(&green);
            k_msleep(500);
        } else {
            k_msleep(100);
        }
    }
}

int conf_buttons(const struct gpio_dt_spec *spec, struct gpio_callback *cb_data, 
                 gpio_callback_handler_t handler, const char *name) {
    int ret;
    if (!gpio_is_ready_dt(spec)) {
        printk("Error: button %s is not ready\n", name);
        return -1;
    }

    ret = gpio_pin_configure_dt(spec, GPIO_INPUT);
    if (ret != 0) {
        printk("Error: failed to configure %s pin\n", name);
        return -1;
    }

    ret = gpio_pin_interrupt_configure_dt(spec, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        printk("Error: failed to configure interrupt on %s\n", name);
        return -1;
    }

    gpio_init_callback(cb_data, handler, BIT(spec->pin));
    gpio_add_callback(spec->port, cb_data);
    printk("Set up button %s ok\n", name);
    
    return 0;
}

int init_buttons(void) {
    conf_buttons(&button_0_spec, &button_0_cb_data, button_0_handler, "button 0");
    conf_buttons(&button_1_spec, &button_1_cb_data, button_1_handler, "button 1");
    conf_buttons(&button_2_spec, &button_2_cb_data, button_2_handler, "button 2");
    conf_buttons(&button_3_spec, &button_3_cb_data, button_3_handler, "button 3");
    conf_buttons(&button_4_spec, &button_4_cb_data, button_4_handler, "button 4");
    return 0;
}

void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { //pause
    printk("Button 0 pressed\n");
    if (led_state != 4) {
        previous_state = led_state;
        led_state = 4;              
    } else {
        led_state = previous_state; 
    }
}

void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { //pysyvä punainen valo
    printk("Button 1 pressed\n");
    gpio_pin_toggle_dt(&red);
}

void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { //pysyvä keltainen valo
    printk("Button 2 pressed\n");
    gpio_pin_toggle_dt(&red);
    gpio_pin_toggle_dt(&green);
}

void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { //pysyvä vihreä valo
    printk("Button 3 pressed\n");
    gpio_pin_toggle_dt(&green);
}

void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) { //keltainen vilkkuminen
    printk("Button 4 pressed\n");
    if (led_state != 5) {
        previous_state = led_state;
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);
        led_state = 5;
    } else {
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);
        led_state = previous_state;
    }
}

int main(void) {
    init_leds();
    init_buttons();
    return 0;
}