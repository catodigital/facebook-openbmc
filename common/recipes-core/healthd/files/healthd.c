/*
 * healthd
 *
 * Copyright 2015-present Facebook. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <pthread.h>
#include <jansson.h>
#include <stdbool.h>
#include <openbmc/pal.h>
#include <sys/sysinfo.h>
#include <sys/reboot.h>
#include "watchdog.h"
#include <openbmc/pal.h>

#define I2C_BUS_NUM            14
#define AST_I2C_BASE           0x1E78A000  /* I2C */
#define I2C_CMD_REG            0x14
#define AST_I2CD_SCL_LINE_STS  (0x1 << 18)
#define AST_I2CD_SDA_LINE_STS  (0x1 << 17)
#define AST_I2CD_BUS_BUSY_STS  (0x1 << 16)
#define PAGE_SIZE              0x1000
#define MIN_THRESHOLD          60.0
#define MAX_THRESHOLD          90.0

struct i2c_bus_s {
  uint32_t offset;
  char     *name;
  bool     enabled;
};
struct i2c_bus_s ast_i2c_dev_offset[I2C_BUS_NUM] = {
  {0x040,  "I2C DEV1 OFFSET", false},
  {0x080,  "I2C DEV2 OFFSET", false},
  {0x0C0,  "I2C DEV3 OFFSET", false},
  {0x100,  "I2C DEV4 OFFSET", false},
  {0x140,  "I2C DEV5 OFFSET", false},
  {0x180,  "I2C DEV6 OFFSET", false},
  {0x1C0,  "I2C DEV7 OFFSET", false},
  {0x300,  "I2C DEV8 OFFSET", false},
  {0x340,  "I2C DEV9 OFFSET", false},
  {0x380,  "I2C DEV10 OFFSET", false},
  {0x3C0,  "I2C DEV11 OFFSET", false},
  {0x400,  "I2C DEV12 OFFSET", false},
  {0x440,  "I2C DEV13 OFFSET", false},
  {0x480,  "I2C DEV14 OFFSET", false},
};

#define CPU_INFO_PATH "/proc/stat"
#define CPU_NAME_LENGTH 10
#define DEFAULT_WINDOW_SIZE 120
#define DEFAULT_MONITOR_INTERVAL 1
#define HEALTHD_MAX_RETRY 10
#define CONFIG_PATH "/etc/healthd-config.json"

struct threshold_s {
  float value;
  float hysteresis;
  bool asserted;
  bool log;
  int log_level;
  bool reboot;
  bool bmc_error_trigger;
};

#define AST_MCR_BASE 0x1e6e0000 // Base Address of SDRAM Memory Controller
#define INTR_CTRL_STS_OFFSET 0x50 // Interrupt Control/Status Register
#define ADDR_FIRST_UNRECOVER_ECC_OFFSET 0x58 // Address of First Un-Recoverable ECC Error Addr
#define ADDR_LAST_RECOVER_ECC_OFFSET 0x5c // Address of Last Recoverable ECC Error Addr
#define MAX_ECC_RECOVERABLE_ERROR_COUNTER 255
#define MAX_ECC_UNRECOVERABLE_ERROR_COUNTER 15

struct ecc_log {
  // to show the address of ecc error or not. supported chip: AST2500 serials
  bool show_addr;
  int regen_interval;
};

#define BMC_HEALTH_FILE "bmc_health"
#define HEALTH "1"
#define NOT_HEALTH "0"

enum ASSERT_BIT {
  BIT_CPU_OVER_THRESHOLD = 0,
  BIT_MEM_OVER_THRESHOLD = 1,
  BIT_RECOVERABLE_ECC    = 2,
  BIT_UNRECOVERABLE_ECC  = 3,
};

/* Heartbeat configuration */
static unsigned int hb_interval = 500;

/* CPU configuration */
static char *cpu_monitor_name = "BMC CPU utilization";
static bool cpu_monitor_enabled = false;
static unsigned int cpu_window_size = DEFAULT_WINDOW_SIZE;
static unsigned int cpu_monitor_interval = DEFAULT_MONITOR_INTERVAL;
static struct threshold_s *cpu_threshold;
static size_t cpu_threshold_num = 0;

/* Memory monitor enabled */
static char *mem_monitor_name = "BMC Memory utilization";
static bool mem_monitor_enabled = false;
static unsigned int mem_window_size = DEFAULT_WINDOW_SIZE;
static unsigned int mem_monitor_interval = DEFAULT_MONITOR_INTERVAL;
static struct threshold_s *mem_threshold;
static size_t mem_threshold_num = 0;

static pthread_mutex_t global_error_mutex = PTHREAD_MUTEX_INITIALIZER;
static int bmc_health = 0; // CPU/MEM/ECC error flag

/* I2C Monitor enabled */
static bool i2c_monitor_enabled = false;

/* ECC configuration */
static char *recoverable_ecc_name = "ECC Recoverable Error";
static char *unrecoverable_ecc_name = "ECC Unrecoverable Error";
static bool ecc_monitor_enabled = false;
static unsigned int ecc_monitor_interval = DEFAULT_MONITOR_INTERVAL;
static struct ecc_log ecc_log_setting = {false, 0};
static struct threshold_s *recov_ecc_threshold;
static size_t recov_ecc_threshold_num = 0;
static struct threshold_s *unrec_ecc_threshold;
static size_t unrec_ecc_threshold_num = 0;
static unsigned int ecc_recov_max_counter = MAX_ECC_RECOVERABLE_ERROR_COUNTER;
static unsigned int ecc_unrec_max_counter = MAX_ECC_UNRECOVERABLE_ERROR_COUNTER;

static void
initialize_threshold(const char *target, json_t *thres, struct threshold_s *t) {
  json_t *tmp;
  size_t i;
  size_t act_size;

  tmp = json_object_get(thres, "value");
  if (!tmp || !json_is_real(tmp)) {
    return;
  }
  t->value = json_real_value(tmp);

  /* Do not let the value (CPU/MEM thresholds) exceed these safe ranges */
  if ((strcasestr(target, "CPU") != 0ULL) ||
      (strcasestr(target, "MEM") != 0ULL)) {
    if (t->value > MAX_THRESHOLD) {
      syslog(LOG_WARNING,
             "%s: user setting %s threshold %f is too high and set threshold as %f",
             __func__, target, t->value, MAX_THRESHOLD);
      t->value = MAX_THRESHOLD;
    }
    if (t->value < MIN_THRESHOLD) {
      syslog(LOG_WARNING,
             "%s: user setting %s threshold %f is too low and set threshold as %f",
             __func__, target, t->value, MIN_THRESHOLD);
      t->value = MIN_THRESHOLD;
    }
  }

  tmp = json_object_get(thres, "hysteresis");
  if (tmp && json_is_real(tmp)) {
    t->hysteresis = json_real_value(tmp);
  }
  tmp = json_object_get(thres, "action");
  if (!tmp || !json_is_array(tmp)) {
    return;
  }
  act_size = json_array_size(tmp);
  for(i = 0; i < act_size; i++) {
    const char *act;
    json_t *act_o = json_array_get(tmp, i);
    if (!act_o || !json_is_string(act_o)) {
      continue;
    }
    act = json_string_value(act_o);
    if (!strcmp(act, "log-warning")) {
      t->log_level = LOG_WARNING;
      t->log = true;
    } else if(!strcmp(act, "log-critical")) {
      t->log_level = LOG_CRIT;
      t->log = true;
    } else if (!strcmp(act, "reboot")) {
      t->reboot = true;
    } else if(!strcmp(act, "bmc-error-trigger")) {
      t->bmc_error_trigger = true;
    }
  }

  if (tmp && json_is_boolean(tmp)) {
    t->reboot = json_is_true(tmp);
  }
}

static void initialize_thresholds(const char *target, json_t *array, struct threshold_s **out_arr, size_t *out_len) {
  size_t size = json_array_size(array);
  size_t i;
  struct threshold_s *thres;

  if (size == 0) {
    return;
  }
  thres = *out_arr = calloc(size, sizeof(struct threshold_s));
  if (!thres) {
    return;
  }
  *out_len = size;

  for(i = 0; i < size; i++) {
    json_t *e = json_array_get(array, i);
    if (!e) {
      continue;
    }
    initialize_threshold(target, e, &thres[i]);
  }
}

static void initialize_ecc_log(json_t *conf, struct ecc_log *log_setting) {
  json_t *tmp;

  tmp = json_object_get(conf, "ecc_address_log");
  if (tmp || json_is_boolean(tmp)) {
    log_setting->show_addr = json_is_true(tmp);
  }
  tmp = json_object_get(conf, "regenerating_interval");
  if (tmp || json_is_number(tmp)) {
    log_setting->regen_interval = json_integer_value(tmp);
  }
}

static void
initialize_hb_config(json_t *conf) {
  json_t *tmp;
  
  tmp = json_object_get(conf, "interval");
  if (!tmp || !json_is_number(tmp)) {
    return;
  }
  hb_interval = json_integer_value(tmp);
}

static void
initialize_cpu_config(json_t *conf) {
  json_t *tmp;

  tmp = json_object_get(conf, "enabled");
  if (!tmp || !json_is_boolean(tmp)) {
    return;
  }
  cpu_monitor_enabled = json_is_true(tmp);
  if (!cpu_monitor_enabled) {
    return;
  }
  tmp = json_object_get(conf, "window_size");
  if (tmp && json_is_number(tmp)) {
    cpu_window_size = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "monitor_interval");
  if (tmp && json_is_number(tmp)) {
    cpu_monitor_interval = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "threshold");
  if (!tmp || !json_is_array(tmp)) {
    cpu_monitor_enabled = false;
    return;
  }
  initialize_thresholds(cpu_monitor_name, tmp, &cpu_threshold, &cpu_threshold_num);
}

static void
initialize_mem_config(json_t *conf) {
  json_t *tmp;

  tmp = json_object_get(conf, "enabled");
  if (!tmp || !json_is_boolean(tmp)) {
    return;
  }
  mem_monitor_enabled = json_is_true(tmp);
  if (!mem_monitor_enabled) {
    return;
  }
  tmp = json_object_get(conf, "window_size");
  if (tmp && json_is_number(tmp)) {
    mem_window_size = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "monitor_interval");
  if (tmp && json_is_number(tmp)) {
    mem_monitor_interval = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "threshold");
  if (!tmp || !json_is_array(tmp)) {
    mem_monitor_enabled = false;
    return;
  }
  initialize_thresholds(mem_monitor_name, tmp, &mem_threshold, &mem_threshold_num);
}

static void
initialize_i2c_config(json_t *conf) {
  json_t *tmp;
  size_t i;
  size_t i2c_num_busses;

  tmp = json_object_get(conf, "enabled");
  if (!tmp || !json_is_boolean(tmp)) {
    return;
  }
  i2c_monitor_enabled = json_is_true(tmp);
  if (!i2c_monitor_enabled) {
    return;
  }
  tmp = json_object_get(conf, "busses");
  if (!tmp || !json_is_array(tmp)) {
    goto error_bail;
  }
  i2c_num_busses = json_array_size(tmp);
  if (!i2c_num_busses) {
    /* Nothing to monitor */
    goto error_bail;
  }
  for(i = 0; i < i2c_num_busses; i++) {
    size_t bus;
    json_t *ind = json_array_get(tmp, i);
    if (!ind || !json_is_number(ind)) {
      goto error_bail;
    }
    bus = json_integer_value(ind);
    if (bus >= I2C_BUS_NUM) {
      syslog(LOG_CRIT, "HEALTHD: Warning: Ignoring unsupported I2C Bus:%u\n", (unsigned int)bus);
      continue;
    }
    ast_i2c_dev_offset[bus].enabled = true;
  }
  return;
error_bail:
  i2c_monitor_enabled = false;
}

static void
initialize_ecc_config(json_t *conf) {
  json_t *tmp;

  tmp = json_object_get(conf, "enabled");
  if (!tmp || !json_is_boolean(tmp)) {
    return;
  }
  ecc_monitor_enabled = json_is_true(tmp);
  if (!ecc_monitor_enabled) {
    return;
  }
  tmp = json_object_get(conf, "log");
  if (tmp || json_is_object(tmp)) {
    initialize_ecc_log(tmp, &ecc_log_setting);
  }
  tmp = json_object_get(conf, "monitor_interval");
  if (tmp && json_is_number(tmp)) {
    ecc_monitor_interval = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "recov_max_counter");
  if (tmp && json_is_number(tmp)) {
    ecc_recov_max_counter = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "unrec_max_counter");
  if (tmp && json_is_number(tmp)) {
    ecc_unrec_max_counter = json_integer_value(tmp);
  }
  tmp = json_object_get(conf, "recov_threshold");
  if (tmp || json_is_array(tmp)) {
    initialize_thresholds(recoverable_ecc_name, tmp, &recov_ecc_threshold, &recov_ecc_threshold_num);
  }
  tmp = json_object_get(conf, "unrec_threshold");
  if (tmp || json_is_array(tmp)) {
    initialize_thresholds(unrecoverable_ecc_name, tmp, &unrec_ecc_threshold, &unrec_ecc_threshold_num);
  }
}

static int
initialize_configuration(void) {
  json_error_t error;
  json_t *conf;
  json_t *v;

  conf = json_load_file(CONFIG_PATH, 0, &error);
  if (!conf) {
    syslog(LOG_CRIT, "HEALTHD configuration load failed");
    return -1;
  }
  v = json_object_get(conf, "version");
  if (v && json_is_string(v)) {
    syslog(LOG_INFO, "Loaded configuration version: %s\n", json_string_value(v));
  }
  initialize_hb_config(json_object_get(conf, "heartbeat"));
  initialize_cpu_config(json_object_get(conf, "bmc_cpu_utilization"));
  initialize_mem_config(json_object_get(conf, "bmc_mem_utilization"));
  initialize_i2c_config(json_object_get(conf, "i2c"));
  initialize_ecc_config(json_object_get(conf, "ecc_monitoring"));

  json_decref(conf);

  return 0;
}

static void threshold_assert_check(const char *target, float value, struct threshold_s *thres) {
  if (!thres->asserted && value >= thres->value) {
    thres->asserted = true;
    if (thres->log) {
      syslog(thres->log_level, "ASSERT: %s (%.2f%%) exceeds the threshold (%.2f%%).\n", target, value, thres->value);
    }
    if (thres->reboot) {
      sleep(1);
      reboot(RB_AUTOBOOT);
    }
    if (thres->bmc_error_trigger) {
      pthread_mutex_lock(&global_error_mutex);
      if (!bmc_health) { // assert bmc_health key only when not yet set
        pal_set_key_value(BMC_HEALTH_FILE, NOT_HEALTH);
      }
      if (strcasestr(target, "CPU") != 0ULL) {
        bmc_health = SETBIT(bmc_health, BIT_CPU_OVER_THRESHOLD);
      } else if (strcasestr(target, "Mem") != 0ULL) {
        bmc_health = SETBIT(bmc_health, BIT_MEM_OVER_THRESHOLD);
      } else {
        pthread_mutex_unlock(&global_error_mutex);
        return;
      }
      pthread_mutex_unlock(&global_error_mutex);
      pal_bmc_err_enable(target);
    }
  }
}

static void threshold_deassert_check(const char *target, float value, struct threshold_s *thres) {
  if (thres->asserted && value < (thres->value - thres->hysteresis)) {
    thres->asserted = false;
    if (thres->log) {
      syslog(thres->log_level, "DEASSERT: %s (%.2f%%) is under the threshold (%.2f%%).\n", target, value, thres->value);
    }
    if (thres->bmc_error_trigger) {
      pthread_mutex_lock(&global_error_mutex);
      if (strcasestr(target, "CPU") != 0ULL) {
        bmc_health = CLEARBIT(bmc_health, BIT_CPU_OVER_THRESHOLD);
      } else if (strcasestr(target, "Mem") != 0ULL) {
        bmc_health = CLEARBIT(bmc_health, BIT_MEM_OVER_THRESHOLD);
      } else {
        pthread_mutex_unlock(&global_error_mutex);
        return;
      }
      if (!bmc_health) { // deassert bmc_health key if no any error bit assertion
        pal_set_key_value(BMC_HEALTH_FILE, HEALTH);
      }
      pthread_mutex_unlock(&global_error_mutex);
      pal_bmc_err_disable(target);
    }
  }
}

static void
threshold_check(const char *target, float value, struct threshold_s *thresholds, size_t num) {
  size_t i;

  for(i = 0; i < num; i++) {
    threshold_assert_check(target, value, &thresholds[i]);
    threshold_deassert_check(target, value, &thresholds[i]);
  }
}

static void ecc_threshold_assert_check(const char *target, int value,
                                       struct threshold_s *thres, uint32_t ecc_err_addr) {
  int thres_counter = 0;

  if (strcasestr(target, "Unrecover") != 0ULL) {
    thres_counter = (ecc_unrec_max_counter * thres->value / 100);
  } else if (strcasestr(target, "Recover") != 0ULL) {
    thres_counter = (ecc_recov_max_counter * thres->value / 100);
  } else {
    return;
  }
  if (!thres->asserted && value > thres_counter) {
    thres->asserted = true;
    if (thres->log) {
      if (ecc_log_setting.show_addr) {
        syslog(LOG_CRIT, "%s occurred (over %d%%) "
            "Counter = %d Address of last recoverable ECC error = 0x%x",
            target, (int)thres->value, value, (ecc_err_addr >> 4) & 0xFFFFFFFF);
      } else {
        syslog(LOG_CRIT, "ECC occurred (over %d%%): %s Counter = %d",
            (int)thres->value, target, value);
      }
    }
    if (thres->reboot) {
      reboot(RB_AUTOBOOT);
    }
    if (thres->bmc_error_trigger) {
      pthread_mutex_lock(&global_error_mutex);
      if (!bmc_health) { // assert in bmc_health key only when not yet set
        pal_set_key_value(BMC_HEALTH_FILE, NOT_HEALTH);
      }
      if (strcasestr(target, "Unrecover") != 0ULL) {
        bmc_health = SETBIT(bmc_health, BIT_UNRECOVERABLE_ECC);
      } else if (strcasestr(target, "Recover") != 0ULL) {
        bmc_health = SETBIT(bmc_health, BIT_RECOVERABLE_ECC);
      } else {
        pthread_mutex_unlock(&global_error_mutex);
        return;
      }
      pthread_mutex_unlock(&global_error_mutex);
      pal_bmc_err_enable(target);
    }
  }
}

static void
ecc_threshold_check(const char *target, int value, struct threshold_s *thresholds,
                    size_t num, uint32_t ecc_err_addr) {
  size_t i;

  for(i = 0; i < num; i++) {
    ecc_threshold_assert_check(target, value, &thresholds[i], ecc_err_addr);
  }
}

static void
initilize_all_kv() {
  pal_set_def_key_value();
}

static void *
hb_handler() {
  while(1) {
    /* Turn ON the HB Led*/
    pal_set_hb_led(1);
    msleep(hb_interval);

    /* Turn OFF the HB led */
    pal_set_hb_led(0);
    msleep(hb_interval);
  }
  return NULL;
}

static void *
watchdog_handler() {

  /* Start watchdog in manual mode */
  start_watchdog(0);

  /* Set watchdog to persistent mode so timer expiry will happen independent
   * of this process's liveliness.
   */
  set_persistent_watchdog(WATCHDOG_SET_PERSISTENT);

  while(1) {

    sleep(5);

    /*
     * Restart the watchdog countdown. If this process is terminated,
     * the persistent watchdog setting will cause the system to reboot after
     * the watchdog timeout.
     */
    kick_watchdog();
  }
  return NULL;
}

static void *
i2c_mon_handler() {
  uint32_t i2c_fd;
  uint32_t i2c_cmd_sts[I2C_BUS_NUM] = {false};
  void *i2c_reg;
  void *i2c_cmd_reg;
  bool is_error_occur[I2C_BUS_NUM] = {false};
  char str_i2c_log[64];
  int timeout;
  int i;

  while (1) {
    i2c_fd = open("/dev/mem", O_RDWR | O_SYNC );
    if (i2c_fd >= 0) {
      i2c_reg = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, i2c_fd, AST_I2C_BASE);
      for (i = 0; i < I2C_BUS_NUM; i++) {
        if (!ast_i2c_dev_offset[i].enabled) {
          continue;
        }
        i2c_cmd_reg = (char*)i2c_reg + ast_i2c_dev_offset[i].offset + I2C_CMD_REG;
        i2c_cmd_sts[i] = *(volatile uint32_t*) i2c_cmd_reg;

        timeout = 3000;
        if ((i2c_cmd_sts[i] & AST_I2CD_SDA_LINE_STS) && !(i2c_cmd_sts[i] & AST_I2CD_SCL_LINE_STS)) {
          //if SDA == 1 and SCL == 0, it means the master is locking the bus.
          if (is_error_occur[i] == false) {
            while (i2c_cmd_sts[i] & AST_I2CD_BUS_BUSY_STS) {
              i2c_cmd_reg = (char*)i2c_reg + ast_i2c_dev_offset[i].offset + I2C_CMD_REG;
              i2c_cmd_sts[i] = *(volatile uint32_t*) i2c_cmd_reg;
              if (timeout < 0) {
                break;
              }
              timeout--;
              usleep(10);
            }
            // If the bus is busy over 30 ms, means the I2C transaction is abnormal.
            // To confirm the bus is not workable.
            if (timeout < 0) {
              memset(str_i2c_log, 0, sizeof(char) * 64);
              sprintf(str_i2c_log, "ASSERT: I2C bus %d crashed (I2C bus index base 0)", i);
              syslog(LOG_CRIT, str_i2c_log);
              is_error_occur[i] = true;
              pal_i2c_crash_assert_handle(i);
            }
          }
        } else {
          if (is_error_occur[i] == true) {
            memset(str_i2c_log, 0, sizeof(char) * 64);
            sprintf(str_i2c_log, "DEASSERT: I2C bus %d crashed (I2C bus index base 0)", i);
            syslog(LOG_CRIT, str_i2c_log);
            is_error_occur[i] = false;
            pal_i2c_crash_deassert_handle(i);
          }
        }
      }
      munmap(i2c_reg, PAGE_SIZE);
      close(i2c_fd);
    }
    sleep(1);
  }
  return NULL;
}

static void *
CPU_usage_monitor() {
  unsigned long long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
  unsigned long long total_diff, idle_diff, non_idle, idle_time = 0, total = 0, pre_total = 0, pre_idle = 0;
  char cpu[CPU_NAME_LENGTH] = {0};
  int i, ready_flag = 0, timer = 0, retry = 0;
  float cpu_util_avg, cpu_util_total;
  float cpu_utilization[cpu_window_size];
  FILE *fp;

  memset(cpu_utilization, 0, sizeof(float) * cpu_window_size);
  
  while (1) {

    if (retry > HEALTHD_MAX_RETRY) {
      syslog(LOG_CRIT, "Cannot get CPU statistics. Stop %s\n", __func__);
      return NULL;
    }

    // Get CPU statistics. Time unit: jiffies
    fp = fopen(CPU_INFO_PATH, "r");
    if(!fp) {
      syslog(LOG_WARNING, "Failed to get CPU statistics.\n");
      retry++;
      continue;
    }
    retry = 0;

    fscanf(fp, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                cpu, &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice);

    fclose(fp);

    timer %= cpu_window_size;

    // Need more data to cacluate the avg. utilization. We average 60 records here.
    if (timer == (cpu_window_size-1) && !ready_flag)
      ready_flag = 1;


    // guset and guest_nice are already accounted in user and nice so they are not included in total caculation
    idle_time = idle + iowait;
    non_idle = user + nice + system + irq + softirq + steal;
    total = idle_time + non_idle;

    // For runtime caculation, we need to take into account previous value.
    total_diff = total - pre_total;
    idle_diff = idle_time - pre_idle;

    // These records are used to caculate the avg. utilization.
    cpu_utilization[timer] = (float) (total_diff - idle_diff)/total_diff;

    // Start to average the cpu utilization
    if (ready_flag) {
      cpu_util_total = 0;
      for (i=0; i<cpu_window_size; i++) {
        cpu_util_total += cpu_utilization[i];
      }
      cpu_util_avg = (cpu_util_total/cpu_window_size) * 100.0;
      threshold_check(cpu_monitor_name, cpu_util_avg, cpu_threshold, cpu_threshold_num);
    }

    // Record current value for next caculation
    pre_total = total;
    pre_idle  = idle_time;

    timer++;
    sleep(cpu_monitor_interval);
  }
  return NULL;
}

static void *
memory_usage_monitor() {
  struct sysinfo s_info;
  int i, error, timer = 0, ready_flag = 0, retry = 0;
  float mem_util_avg, mem_util_total;
  float mem_utilization[mem_window_size];

  memset(mem_utilization, 0, sizeof(float) * mem_window_size);

  while (1) {

    if (retry > HEALTHD_MAX_RETRY) {
      syslog(LOG_CRIT, "Cannot get sysinfo. Stop the %s\n", __func__);
      return NULL;
    }

    timer %= mem_window_size;

    // Need more data to cacluate the avg. utilization. We average 60 records here.
    if (timer == (mem_window_size-1) && !ready_flag)
      ready_flag = 1;

    // Get sys info
    error = sysinfo(&s_info);
    if (error) {
      syslog(LOG_WARNING, "%s Failed to get sys info. Error: %d\n", __func__, error);
      retry++;
      continue;
    }
    retry = 0;

    // These records are used to caculate the avg. utilization.
    mem_utilization[timer] = (float) (s_info.totalram - s_info.freeram)/s_info.totalram;

    // Start to average the memory utilization
    if (ready_flag) {
      mem_util_total = 0;
      for (i=0; i<mem_window_size; i++)
        mem_util_total += mem_utilization[i];

      mem_util_avg = (mem_util_total/mem_window_size) * 100.0;

      threshold_check(mem_monitor_name, mem_util_avg, mem_threshold, mem_threshold_num);
    }

    timer++;
    sleep(mem_monitor_interval);
  }
  return NULL;
}

// Thread to monitor the ECC counter
static void *
ecc_mon_handler() {
  uint32_t mcr_fd = 0;
  uint32_t ecc_status = 0;
  uint32_t unrecover_ecc_err_addr = 0;
  uint32_t recover_ecc_err_addr = 0;
  uint16_t ecc_recoverable_error_counter = 0;
  uint8_t ecc_unrecoverable_error_counter = 0;
  void *mcr_base_addr;
  void *mcr50_addr;
  void *mcr58_addr;
  void *mcr5c_addr;
  int bmc_health_last_state = 1;
  int bmc_health_kv_state = 1;
  char tmp_health[MAX_VALUE_LEN];
  int ret = 0;
  int relog_counter = 0;
  int retry_err = 0;

  while (1) {
    mcr_fd = open("/dev/mem", O_RDWR | O_SYNC );
    if (mcr_fd < 0) {
      // In case of error opening the file, sleep for 2 sec and retry.
      // During continuous failures, log the error every 20 minutes.
      sleep(2);
      if (++retry_err >= 600) {
        syslog(LOG_ERR, "%s - cannot open /dev/mem", __func__);
        retry_err = 0;
      }
      continue;
    }

    retry_err = 0;

    mcr_base_addr = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mcr_fd,
        AST_MCR_BASE);
    mcr50_addr = (char*)mcr_base_addr + INTR_CTRL_STS_OFFSET;
    ecc_status = *(volatile uint32_t*) mcr50_addr;
    if (ecc_log_setting.show_addr) {
      mcr58_addr = (char*)mcr_base_addr + ADDR_FIRST_UNRECOVER_ECC_OFFSET;
      unrecover_ecc_err_addr = *(volatile uint32_t*) mcr58_addr;
      mcr5c_addr = (char*)mcr_base_addr + ADDR_LAST_RECOVER_ECC_OFFSET;
      recover_ecc_err_addr = *(volatile uint32_t*) mcr5c_addr;
    }
    munmap(mcr_base_addr, PAGE_SIZE);
    close(mcr_fd);

    // get current health status from kv_store
    memset(tmp_health, 0, MAX_VALUE_LEN);
    ret = pal_get_key_value(BMC_HEALTH_FILE, tmp_health);
    if (ret){
      syslog(LOG_ERR, " %s - kv get bmc_health status failed", __func__);
    }

    bmc_health_kv_state = atoi(tmp_health);

    ecc_recoverable_error_counter = (ecc_status >> 16) & 0xFF;
    ecc_unrecoverable_error_counter = (ecc_status >> 12) & 0xF;

    // Check ECC recoverable error counter
    ecc_threshold_check(recoverable_ecc_name, ecc_recoverable_error_counter,
                        recov_ecc_threshold, recov_ecc_threshold_num, recover_ecc_err_addr);

    // Check ECC un-recoverable error counter
    ecc_threshold_check(unrecoverable_ecc_name, ecc_unrecoverable_error_counter,
                        unrec_ecc_threshold, unrec_ecc_threshold_num, unrecover_ecc_err_addr);

    // If log-util clear all fru, cleaning ECC error status
    // After doing it, daemon will regenerate assert
    // Generage a syslog every regen_interval loop counter
    if (((ecc_log_setting.regen_interval > 0) &&
         (relog_counter >= ecc_log_setting.regen_interval)) ||
        ((bmc_health_kv_state != bmc_health_last_state) &&
         (bmc_health_kv_state == 1))) {
      bmc_health = CLEARBIT(bmc_health, BIT_RECOVERABLE_ECC);
      bmc_health = CLEARBIT(bmc_health, BIT_UNRECOVERABLE_ECC);
      relog_counter = 0;
    }
    bmc_health_last_state = bmc_health_kv_state;
    relog_counter++;
    sleep(ecc_monitor_interval);
  }
  return NULL;
}

void
fw_update_ongoing(int update_status)
{
  // update_status = 1, it implies that fw update is on-going
  if (update_status == 1) {
    system("chmod 666 /usr/local/fbpackages/power-util/power-util");
    system("chmod 666 /sbin/halt.sysvinit");
    system("chmod 666 /sbin/shutdown.sysvinit");
  }
  else {
    system("chmod 755 /usr/local/fbpackages/power-util/power-util");
    system("chmod 4755 /sbin/halt.sysvinit");
    system("chmod 4755 /sbin/shutdown.sysvinit");
  }
}

static void *
fw_update_monitor() {
  int fw_update_flag, prev_val = 0;
  int counter=0, counter_is_start=false;

  while(1) {
    //TODO: Change to use save flag in kv,
    //when kv save value in RAMDisk function is avaliable.

    //if fw_update_flag = 1 means BMC is Updating a Device FW  
    fw_update_flag = pal_get_fw_update_flag();
    if (fw_update_flag != prev_val) {
      if (fw_update_flag) {
        fw_update_ongoing(1);
        //Start Counter
        counter_is_start = true;
        counter = 0;
      }
      else {
        fw_update_ongoing(0);

        counter_is_start = false;
      }
    }
    prev_val = fw_update_flag;
    sleep(1);

    //Timer Counter to enable permission
    if (counter_is_start) {
      // Wait for a maximum time of 3000 seconds before
      // exiting the BMC FW update mode
      if (counter < 3000) {
        counter++;
      } else {
        if (pal_remove_fw_update_flag()) {
          syslog(LOG_WARNING, "%s: failed to remove update flag", __func__);
        }
      }
    }
  }
  return NULL;
}

int
main(int argc, char **argv) {
  pthread_t tid_watchdog;
  pthread_t tid_hb_led;
  pthread_t tid_i2c_mon;
  pthread_t tid_cpu_monitor;
  pthread_t tid_mem_monitor;
  pthread_t tid_fw_update_monitor;
  pthread_t tid_ecc_monitor;

  if (argc > 1) {
    exit(1);
  }

  initilize_all_kv();

  initialize_configuration();


// For current platforms, we are using WDT from either fand or fscd
// TODO: keeping this code until we make healthd as central daemon that
//  monitors all the important daemons for the platforms.
  if (pthread_create(&tid_watchdog, NULL, watchdog_handler, NULL) < 0) {
    syslog(LOG_WARNING, "pthread_create for watchdog error\n");
    exit(1);
  }

  if (pthread_create(&tid_hb_led, NULL, hb_handler, NULL) < 0) {
    syslog(LOG_WARNING, "pthread_create for heartbeat error\n");
    exit(1);
  }

  if (cpu_monitor_enabled) {
    if (pthread_create(&tid_cpu_monitor, NULL, CPU_usage_monitor, NULL) < 0) {
      syslog(LOG_WARNING, "pthread_create for monitor CPU usage\n");
      exit(1);
    }
  }

  if (mem_monitor_enabled) {
    if (pthread_create(&tid_mem_monitor, NULL, memory_usage_monitor, NULL) < 0) {
      syslog(LOG_WARNING, "pthread_create for monitor memory usage\n");
      exit(1);
    }
  }

  if (i2c_monitor_enabled) {
    // Add a thread for monitoring all I2C buses crash or not
    if (pthread_create(&tid_i2c_mon, NULL, i2c_mon_handler, NULL) < 0) {
      syslog(LOG_WARNING, "pthread_create for I2C errorr\n");
      exit(1);
    }
  }

  if (ecc_monitor_enabled) {
    if (pthread_create(&tid_ecc_monitor, NULL, ecc_mon_handler, NULL) < 0) {
      syslog(LOG_WARNING, "pthread_create for ECC monitoring errorr\n");
      exit(1);
    }
  }

  if (pthread_create(&tid_fw_update_monitor, NULL, fw_update_monitor, NULL) < 0) {
    syslog(LOG_WARNING, "pthread_create for FW Update Monitor error\n");
    exit(1);
  }

  pthread_join(tid_watchdog, NULL);

  pthread_join(tid_hb_led, NULL);

  if (i2c_monitor_enabled) {
    pthread_join(tid_i2c_mon, NULL);
  }
  if (cpu_monitor_enabled) {
    pthread_join(tid_cpu_monitor, NULL);
  }

  if (mem_monitor_enabled) {
    pthread_join(tid_mem_monitor, NULL);
  }

  if (ecc_monitor_enabled) {
    pthread_join(tid_ecc_monitor, NULL);
  }

  pthread_join(tid_fw_update_monitor, NULL);

  return 0;
}

