#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include "engine.h"

LOG_MODULE_REGISTER(rrgb_state, CONFIG_LOG_DEFAULT_LEVEL);

#define RRGB_KEY  "rainy_rgb/state"

/* RGB params change interactively (hold-to-adjust hue/brightness), so use a
 * short save debounce instead of ZMK's 60s global (CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE)
 * — otherwise a change followed by a reboot within 60s never commits to NVS. */
#define RRGB_SAVE_DEBOUNCE_MS 2000

static int rrgb_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "state", &next) && !next) {
        if (len == sizeof(struct rrgb_persist)) {
            struct rrgb_persist p;
            int rc = read_cb(cb_arg, &p, sizeof(p));
            if (rc >= 0) { rrgb_set_persist(&p); return 0; }
            return rc;
        } else if (len == sizeof(struct rrgb_persist_v1)) {
            struct rrgb_persist_v1 p1;
            int rc = read_cb(cb_arg, &p1, sizeof(p1));
            if (rc >= 0) { rrgb_set_persist_v1(&p1); return 0; }
            return rc;
        }
        return -EINVAL;
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(rainy_rgb, "rainy_rgb", NULL, rrgb_set, NULL, NULL);

static void save_work_fn(struct k_work *w) {
    ARG_UNUSED(w);
    struct rrgb_persist p;
    rrgb_get_persist(&p);
    settings_save_one(RRGB_KEY, &p, sizeof(p));
    LOG_INF("persisted: effect=%u on=%u val=%u side_on=%u logo_on=%u",
            p.effect, p.on, p.val, p.side_on, p.logo_on);
}
static K_WORK_DELAYABLE_DEFINE(rrgb_save_work, save_work_fn);

void rrgb_request_save(void) {
    k_work_reschedule(&rrgb_save_work, K_MSEC(RRGB_SAVE_DEBOUNCE_MS));
}
