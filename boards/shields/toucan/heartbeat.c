// Diagnostic heartbeat for the right-half freeze investigation.
//
// One line per second to printk (USB CDC ACM when paired with the
// zmk-usb-logging snippet):
//
//   [HB <tick>] up=<ms> wq=<sysworkq counter> bt=<conn count>
//               sys=<sysworkq state> pn=<spi-in>/<spi-out>
//
// `tick` and `up` come from a dedicated thread that has its own stack
// and runs independently of the system workqueue. `wq` is incremented
// by a work item submitted to sysworkq once per tick. `bt` is the
// number of currently established LE connections. `sys` is the kernel
// state of the sysworkq thread. `pn` counts cirque SPI transactions
// entered/exited (strong override of the weak hooks in the pinnacle
// driver). If pn=N/N-1 persists during a freeze, an SPI transceive
// is stuck and that's the hang site.
//
// This build also routes pinnacle_work_cb onto its own dedicated work
// queue (via the weak pinnacle_work_q() hook) so a hang in the trackpad
// driver doesn't take down sysworkq. Keys and BLE should keep working
// even when the trackpad is wedged.

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/conn.h>

#define HEARTBEAT_STACK_SIZE 768
#define HEARTBEAT_PRIORITY   5  // preemptible; runs whenever sysworkq is blocked on a wait

static atomic_t wq_counter;

static void wq_probe_handler(struct k_work *work) {
    ARG_UNUSED(work);
    atomic_inc(&wq_counter);
}

static K_WORK_DEFINE(wq_probe, wq_probe_handler);

static void count_cb(struct bt_conn *conn, void *ud) {
    // bt_conn_foreach also iterates disconnected-but-not-yet-freed conns
    // and ones in the CONNECTING state, so filter for the real thing.
    struct bt_conn_info info;
    if (bt_conn_get_info(conn, &info) == 0 && info.state == BT_CONN_STATE_CONNECTED) {
        (*(unsigned *)ud)++;
    }
}

static unsigned bt_conn_count(void) {
    unsigned n = 0;
    bt_conn_foreach(BT_CONN_TYPE_LE, count_cb, &n);
    return n;
}

extern struct k_work_q k_sys_work_q;

static const char *sysworkq_state(char *buf, size_t buf_size) {
    return k_thread_state_str((k_tid_t)&k_sys_work_q.thread, buf, buf_size);
}

// Strong overrides of the weak hooks in cirque-input-module/.../input_pinnacle.c.
// SPI lifecycle counters distinguish "stuck inside spi_transceive_dt" from
// "stuck elsewhere in pinnacle_work_cb."
static atomic_t pn_spi_in;
static atomic_t pn_spi_out;

void pinnacle_instrument_spi_enter(void) { atomic_inc(&pn_spi_in); }
void pinnacle_instrument_spi_exit(void)  { atomic_inc(&pn_spi_out); }

// Dedicated work queue for pinnacle_work_cb. Submitting trackpad work
// here instead of sysworkq means an SPI hang only wedges this queue;
// keyscan and BLE stay responsive.
//
// Started at POST_KERNEL/0 (well before INPUT_INIT_PRIORITY, the level
// at which pinnacle_init runs) so the queue exists by the time the
// first DR interrupt fires and pinnacle_gpio_cb hits pinnacle_work_q().
#define PINNACLE_WORKQ_STACK_SIZE 1024
#define PINNACLE_WORKQ_PRIORITY   6   // preemptible, below BT thread

K_THREAD_STACK_DEFINE(pinnacle_workq_stack, PINNACLE_WORKQ_STACK_SIZE);
static struct k_work_q pinnacle_workq;

struct k_work_q *pinnacle_work_q(void) {
    return &pinnacle_workq;
}

static int pinnacle_workq_start(void) {
    k_work_queue_init(&pinnacle_workq);
    k_work_queue_start(&pinnacle_workq, pinnacle_workq_stack,
                       K_THREAD_STACK_SIZEOF(pinnacle_workq_stack),
                       PINNACLE_WORKQ_PRIORITY, NULL);
    return 0;
}

SYS_INIT(pinnacle_workq_start, POST_KERNEL, 0);

static void heartbeat_thread(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    // Let USB CDC enumerate before flooding the console.
    k_msleep(2000);

    uint32_t tick = 0;
    char state_buf[16];
    for (;;) {
        k_work_submit(&wq_probe);
        k_msleep(1000);
        printk("[HB %u] up=%lldms wq=%ld bt=%u sys=%s pn=%ld/%ld\n",
               tick++,
               (long long)k_uptime_get(),
               (long)atomic_get(&wq_counter),
               bt_conn_count(),
               sysworkq_state(state_buf, sizeof(state_buf)),
               (long)atomic_get(&pn_spi_in),
               (long)atomic_get(&pn_spi_out));
    }
}

K_THREAD_DEFINE(toucan_heartbeat_tid, HEARTBEAT_STACK_SIZE,
                heartbeat_thread, NULL, NULL, NULL,
                HEARTBEAT_PRIORITY, 0, 0);
