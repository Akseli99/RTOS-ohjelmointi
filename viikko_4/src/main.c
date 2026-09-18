/* Tavoittelin tässä tehtävässä 3 pistettä. Debug moden saa päälle kirjoittamalla D,1 terminaaliin. D,0 = pois päältä. 
Valo taskien debug viestit lähetetään taskien sisältä erilliseen debug-puskuriin*/
#include <stdlib.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/timing/timing.h>
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

// UART initialization
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

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

/*static struct k_mutex led_mutex;
static struct k_condvar red_condvar;
static struct k_condvar yellow_condvar;
static struct k_condvar green_condvar;
static struct k_condvar release_condvar;*/

volatile char current_color = '\0';
volatile bool task_completed = false;
volatile int led_state = 0; // 0 = punainen, 1 = keltainen, 2 = vihreä, 4 = pause, 5 = vilkkuminen
volatile int previous_state = 0;
volatile int current_duration = 0;
volatile bool debug_enabled = true;

// Funktioiden prototyypit
void red_led_task(void *, void *, void *);
void yellow_led_task(void *, void *, void *);
void green_led_task(void *, void *, void *);
void blink_yellow_task(void *, void *, void *);
static void dispatcher_task(void *, void *, void *);
static void uart_task(void *, void *, void *);
void debug_task(void *, void *, void*);

void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(blink_thread, STACKSIZE, blink_yellow_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dis_thread,STACKSIZE,dispatcher_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(uart_thread,STACKSIZE,uart_task,NULL,NULL,NULL,PRIORITY,0,0);
K_THREAD_DEFINE(debug_thread,STACKSIZE,debug_task,NULL,NULL,NULL,PRIORITY,0,0);

K_MUTEX_DEFINE(led_mutex);
K_CONDVAR_DEFINE(red_condvar);
K_CONDVAR_DEFINE(yellow_condvar);
K_CONDVAR_DEFINE(green_condvar);
K_CONDVAR_DEFINE(release_condvar);



// dispatcher FIFO buffer
K_FIFO_DEFINE(data_fifo);
K_FIFO_DEFINE(debug_fifo);

struct data_t {
	void *fifo_reserved;
	char msg[40];
    uint64_t time;
};

struct led_command {
    void *fifo_reserved; 
    char color;          // R/r, G,g tai Y/y
    int duration;        // Kuinka kauan valo pysyy päällä
};

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

int init_uart(void) {
	// UART initialization
	if (!device_is_ready(uart_dev)) {
          printk("UART init error\n");
		return -ENODEV;
	} 
    printk("UART init complete!\n");
    return 0;
}

static void uart_task(void *unused1, void *unused2, void *unused3)
{
	// Received character from UART
	char rc=0;
	// Message from UART
	char uart_msg[20];
	memset(uart_msg,0,20);
	int uart_msg_cnt = 0;

	while (true) {
		// Ask UART if data available
		if (uart_poll_in(uart_dev,&rc) == 0) {
			printk("Received: %c\n",rc);
			// If character is not newline, add to UART message buffer
			if (rc != '\r' && rc != '\n') {
				uart_msg[uart_msg_cnt] = rc;
				uart_msg_cnt++;
			// Character is newline, copy dispatcher data and put to FIFO buffer
			} else {
                if (uart_msg_cnt > 0){
                    uart_msg[uart_msg_cnt] = '\0';
                    printk("UART msg: %s\n", uart_msg);
                    
                    struct data_t *buf = k_malloc(sizeof(struct data_t));
                    if (buf != NULL) {
                        snprintf(buf->msg, sizeof(buf->msg), "%s", uart_msg);
                        k_fifo_put(&data_fifo, buf);
                    }
                    // Copy UART message to dispatcher data
                    // strncpy(buf->msg, 20, uart_msg); // mitä ihmettä, miksi kaatuu

                    // Clear UART receive buffer
                    uart_msg_cnt = 0;
                    memset(uart_msg,0,20);
                }
			}
		}
		k_msleep(10);
	}
}

static void dispatcher_task(void *unused1, void *unused2, void *unused3)
{
	while (true) {
		// Receive dispatcher data from uart_task fifo
		struct data_t *rec_item = k_fifo_get(&data_fifo, K_FOREVER);
		char sequence[50];
		strncpy(sequence,rec_item->msg,sizeof(sequence));
		k_free(rec_item);

		printk("Dispatcher: %s\n", sequence);
        char *token = strtok(sequence, ",");
        while (token != NULL) {
            char color = token[0]; 
           
            if (color == 'D' || color == 'd') {
                token = strtok(NULL, ",");
                if (token != NULL) {
                    int state = atoi(token);
                    debug_enabled = (state != 0);
                    printk("Debug mode %s\n", debug_enabled ? "ON" : "OFF");
                }
                token = strtok(NULL, ",");
                continue;
            }
            token = strtok(NULL, ",");
            if (token == NULL) break;
            int duration = atoi(token);

            k_mutex_lock(&led_mutex, K_FOREVER);
            current_color = color;
            current_duration = duration; 
            task_completed = false;
            if (color == 'R' || color == 'r') {
                k_condvar_signal(&red_condvar);
            } else if (color == 'Y' || color == 'y') {
                k_condvar_signal(&yellow_condvar);
            } else if (color == 'G' || color == 'g') {
                k_condvar_signal(&green_condvar);
            }
            while (!task_completed) {
                k_condvar_wait(&release_condvar, &led_mutex, K_FOREVER);
            }
            k_mutex_unlock(&led_mutex);
            token = strtok(NULL, ",");
        }
        printk("Sequence complete.\n");
	}
}

void debug_task(void *, void *, void*) {

	// Store received data
	struct data_t *received;
	while (true) {

		received = k_fifo_get(&debug_fifo, K_FOREVER);
		printk("Debug msg: %s. Time: %llu\n",received->msg, received->time);
		k_free(received);

		k_yield();
	}
}

void red_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        while (current_color != 'R' && current_color != 'r') {
            k_condvar_wait(&red_condvar, &led_mutex, K_FOREVER);
        }
        
        timing_t red_start_time = timing_counter_get(); //Aloitetaan ajan mittaus

        int duration = current_duration;
        
        struct data_t *msg_on = k_malloc(sizeof(struct data_t));
        if (msg_on != NULL) {
            snprintf(msg_on->msg, sizeof(msg_on->msg), "Red ON for %d ms", duration);
            msg_on->time = 0;
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, msg_on);
            } 
            else {
                k_free(msg_on); 
            }
        }

        gpio_pin_set_dt(&red, 1);
        k_msleep(duration);
        gpio_pin_set_dt(&red, 0);

        struct data_t *msg_off = k_malloc(sizeof(struct data_t));
        if (msg_off != NULL) {
            snprintf(msg_off->msg, sizeof(msg_off->msg), "Red LED OFF");
            msg_off->time = 0;
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, msg_off);
            } 
            else {
                k_free(msg_off); 
            }
        }

        current_color = '\0';
        task_completed = true;
        k_condvar_signal(&release_condvar);

        timing_t red_end_time = timing_counter_get();
        uint64_t total_cycles = timing_cycles_get(&red_start_time, &red_end_time);
        uint64_t timing_us = timing_cycles_to_ns(total_cycles) / 1000; //Aika muutetaan mikrosekunneiksi
        struct data_t *debug_data = k_malloc(sizeof(struct data_t));    // Lähetetään mitattu aika debug-taskille
        if (debug_data != NULL) {
            debug_data->time = timing_us;
            snprintf(debug_data->msg, sizeof(debug_data->msg), "Red total time:");
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, debug_data);
            } 
            else {
                k_free(debug_data); 
            }
}

        k_mutex_unlock(&led_mutex);
        k_yield();
    }
}

void yellow_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        while (current_color != 'Y' && current_color != 'y') {
            k_condvar_wait(&yellow_condvar, &led_mutex, K_FOREVER);
        }
        
        timing_t yellow_start_time = timing_counter_get();
        int duration = current_duration;

        struct data_t *msg_on = k_malloc(sizeof(struct data_t));
        if (msg_on != NULL) {
            snprintf(msg_on->msg, sizeof(msg_on->msg), "Yellow ON for %d ms", duration);
            msg_on->time = 0;
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, msg_on);
            } 
            else {
                k_free(msg_on); 
            }
        }


        gpio_pin_set_dt(&red, 1);
        gpio_pin_set_dt(&green, 1);
        k_msleep(duration);
        gpio_pin_set_dt(&red, 0);
        gpio_pin_set_dt(&green, 0);

        struct data_t *msg_off = k_malloc(sizeof(struct data_t));
        if (msg_off != NULL) {
            snprintf(msg_off->msg, sizeof(msg_off->msg), "Yellow LED OFF");
            msg_off->time = 0;
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, msg_off);
            } 
            else {
                k_free(msg_off); 
            }
        }

        current_color = '\0';
        task_completed = true;
        k_condvar_signal(&release_condvar);

        timing_t yellow_end_time = timing_counter_get();
        uint64_t total_cycles = timing_cycles_get(&yellow_start_time, &yellow_end_time);
        uint64_t timing_us = timing_cycles_to_ns(total_cycles) / 1000;

        struct data_t *debug_data = k_malloc(sizeof(struct data_t));
        if (debug_data != NULL) {
            debug_data->time = timing_us;
            snprintf(debug_data->msg, sizeof(debug_data->msg), "Yellow total time:");
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, debug_data);
            } 
            else {
                k_free(debug_data); 
            }
        }

        k_mutex_unlock(&led_mutex);
        k_yield();
    }
}

void green_led_task(void *p1, void *p2, void *p3) {
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        while (current_color != 'G' && current_color != 'g') {
            k_condvar_wait(&green_condvar, &led_mutex, K_FOREVER);
        }
        
        timing_t green_start_time = timing_counter_get();
        int duration = current_duration;

        struct data_t *msg_on = k_malloc(sizeof(struct data_t));
        if (msg_on != NULL) {
            snprintf(msg_on->msg, sizeof(msg_on->msg), "Green ON for %d ms", duration);
            msg_on->time = 0;
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, msg_on);
            } 
            else {
                k_free(msg_on); 
            }
        }

        gpio_pin_set_dt(&green, 1);
        k_msleep(duration);
        gpio_pin_set_dt(&green, 0);

        struct data_t *msg_off = k_malloc(sizeof(struct data_t));
        if (msg_off != NULL) {
            snprintf(msg_off->msg, sizeof(msg_off->msg), "Green LED OFF");
            msg_off->time = 0;
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, msg_off);
            } 
            else {
                k_free(msg_off); 
            }
        }

        current_color = '\0';
        task_completed = true;
        k_condvar_signal(&release_condvar);

        timing_t green_end_time = timing_counter_get();
        uint64_t total_cycles = timing_cycles_get(&green_start_time, &green_end_time);
        uint64_t timing_us = timing_cycles_to_ns(total_cycles) / 1000;

        struct data_t *debug_data = k_malloc(sizeof(struct data_t));
        if (debug_data != NULL) {
            debug_data->time = timing_us;
            snprintf(debug_data->msg, sizeof(debug_data->msg), "Green total time:");
            if (debug_enabled) {  //Tarkistetaan onko debug flagi päällä
                k_fifo_put(&debug_fifo, debug_data);
            } 
            else {
                k_free(debug_data); 
            }
        }

        k_mutex_unlock(&led_mutex);
        k_yield();
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
    init_uart();
    timing_init();
    return 0;
}