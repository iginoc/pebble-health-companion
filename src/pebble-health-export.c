#include <inttypes.h>
#include <pebble.h>
#include "dict_tools.h"
#include "progress_layer.h"

#define MSG_KEY_LAST_SENT	110
#define MSG_KEY_UPLOAD_DONE	130
#define MSG_KEY_UPLOAD_START	140
#define MSG_KEY_DATA_KEY	210
#define MSG_KEY_DATA_LINE	220

#define MSG_KEY_DAY_DATE    310
#define MSG_KEY_DAY_STEPS   320
#define MSG_KEY_DAY_SLEEP   330
#define MSG_KEY_DAY_DEEP    340

#define PERSIST_KEY_LAST_SENT 10000

static Window *window;
static TextLayer *status_layer;
static ProgressLayer *progress_layer;
static HealthMinuteData minute_data[60];
static HealthActivityMask minute_activity[60];
static uint16_t minute_data_size = 0;
static uint16_t minute_index = 0;
static time_t minute_first = 0;
static time_t last_time_requested = 0;
static bool sending = false;
static char global_buffer[128];
static int days_sent = 0;

static void send_next_line();

static void set_status(const char *msg) {
    text_layer_set_text(status_layer, msg);
}

// Funzione per inviare i totali giornalieri precisi usando la finestra 18:00 - 18:00
static void send_daily_totals() {
    if (days_sent >= 7) {
        set_status("Dettagli...");
        minute_first = last_time_requested + 60;
        minute_data_size = 0;
        minute_index = 0;
        send_next_line();
        return;
    }

    // Pebble Health Day: dalle 18:00 alle 18:00
    // Usiamo localtime per calcolare l'inizio del giorno corrente
    time_t now = time(NULL);
    struct tm *today_tm = localtime(&now);
    today_tm->tm_hour = 0;
    today_tm->tm_min = 0;
    today_tm->tm_sec = 0;
    time_t target_day_start = mktime(today_tm) - (days_sent * SECONDS_PER_DAY);

    // Finestra Sonno: dalle 18:00 del giorno precedente alle 18:00 del giorno target
    time_t sleep_start = target_day_start - (6 * 3600);
    time_t sleep_end = target_day_start + (18 * 3600);

    // Finestra Passi: giornata solare 00:00 - 24:00
    time_t steps_start = target_day_start;
    time_t steps_end = target_day_start + SECONDS_PER_DAY;

    HealthValue steps = health_service_sum(HealthMetricStepCount, steps_start, steps_end);
    HealthValue sleep = health_service_sum(HealthMetricSleepSeconds, sleep_start, sleep_end) / 60;
    HealthValue deep = health_service_sum(HealthMetricSleepRestfulSeconds, sleep_start, sleep_end) / 60;

    
    struct tm *t = localtime(&target_day_start);
    char date_buf[16];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", t);

    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
        dict_write_cstring(iter, MSG_KEY_DAY_DATE, date_buf);
        dict_write_int(iter, MSG_KEY_DAY_STEPS, &steps, 4, true);
        dict_write_int(iter, MSG_KEY_DAY_SLEEP, &sleep, 4, true);
        dict_write_int(iter, MSG_KEY_DAY_DEEP, &deep, 4, true);
        days_sent++;
        app_message_outbox_send();
    }
}

static uint16_t format_line(char *buffer, size_t size, HealthMinuteData *data, HealthActivityMask activity, time_t key) {
    struct tm *tm = localtime(&key);
    size_t ret = strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
    int sleep = (activity & HealthActivityRestfulSleep) ? 2 : ((activity & HealthActivitySleep) ? 1 : 0);
    return ret + snprintf(buffer + ret, size - ret, ",%u,%d,%u", data->is_invalid ? 0 : data->steps, sleep, data->is_invalid ? 0 : data->heart_rate_bpm);
}

static bool record_activity(HealthActivity activity, time_t start, time_t end, void *context) {
    uint16_t first = (start <= minute_first) ? 0 : (start - minute_first) / 60;
    uint16_t last = (end - minute_first + 59) / 60;
    if (first < minute_data_size) {
        if (last > minute_data_size) last = minute_data_size;
        for (uint16_t i = first; i < last; i++) minute_activity[i] |= activity;
    }
    return true;
}

static void load_page(time_t start) {
    minute_first = start;
    time_t end = time(NULL);
    if (minute_first >= end) { minute_data_size = 0; return; }
    minute_data_size = health_service_get_minute_history(minute_data, 60, &minute_first, &end);
    minute_index = 0;
    memset(minute_activity, 0, sizeof(minute_activity));
    health_service_activities_iterate(HealthActivityMaskAll, minute_first, end, HealthIterationDirectionFuture, record_activity, NULL);
}

static void send_next_line() {
    if (minute_index >= minute_data_size) {
        load_page(minute_first + (minute_data_size * 60));
        if (minute_data_size == 0) {
            sending = false; set_status("Sync Fine");
            DictionaryIterator *iter;
            if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
                uint32_t last_key = (minute_first / 60);
                dict_write_int(iter, MSG_KEY_UPLOAD_DONE, &last_key, 4, true);
                app_message_outbox_send();
            }
            return;
        }
    }

    DictionaryIterator *iter;
    if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
        time_t current_time = minute_first + (minute_index * 60);
        uint32_t key = current_time / 60;
        format_line(global_buffer, sizeof(global_buffer), &minute_data[minute_index], minute_activity[minute_index], current_time);
        dict_write_int(iter, MSG_KEY_DATA_KEY, &key, 4, true);
        dict_write_cstring(iter, MSG_KEY_DATA_LINE, global_buffer);
        minute_index++;
        app_message_outbox_send();
        int progress = (current_time - last_time_requested) * 100 / (time(NULL) - last_time_requested + 1);
        progress_layer_set_progress(progress_layer, progress > 100 ? 100 : progress);
    }
}

static void inbox_received(DictionaryIterator *iter, void *context) {
    Tuple *t = dict_find(iter, MSG_KEY_LAST_SENT);
    if (t) {
        last_time_requested = t->value->uint32;
        if (last_time_requested == 1) { persist_delete(PERSIST_KEY_LAST_SENT); last_time_requested = 0; }
        else if (last_time_requested == 0 && persist_exists(PERSIST_KEY_LAST_SENT)) { last_time_requested = persist_read_int(PERSIST_KEY_LAST_SENT); }

        days_sent = 0;
        sending = true;
        minute_data_size = 0;
        minute_index = 0;
        set_status("Totali...");
        send_daily_totals();
    }

    if (dict_find(iter, MSG_KEY_UPLOAD_DONE)) {
        uint32_t ack = dict_find(iter, MSG_KEY_UPLOAD_DONE)->value->uint32;
        persist_write_int(PERSIST_KEY_LAST_SENT, ack * 60);
        set_status("Sync OK");
    }
}

static void outbox_sent(DictionaryIterator *iter, void *context) {
    if (sending) {
        if (days_sent < 7 && !minute_data_size) send_daily_totals();
        else send_next_line();
    }
}

static void init() {
    window = window_create();
    Layer *root = window_get_root_layer(window);
    status_layer = text_layer_create(GRect(0, 40, 144, 30));
    text_layer_set_text_alignment(status_layer, GTextAlignmentCenter);
    text_layer_set_text(status_layer, "Pronto");
    layer_add_child(root, text_layer_get_layer(status_layer));
    progress_layer = progress_layer_create(GRect(20, 80, 104, 10));
    layer_add_child(root, progress_layer);
    app_message_register_inbox_received(inbox_received);
    app_message_register_outbox_sent(outbox_sent);
    app_message_open(256, 256);
    window_stack_push(window, true);
}

int main() { init(); app_event_loop(); }
