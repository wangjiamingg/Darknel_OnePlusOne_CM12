/*
 * drivers/input/touchscreen/sweep2wake.c
 *
 *
 * Copyright (c) 2013, Dennis Rassmann <showp1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/input/sweep2wake.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#ifdef CONFIG_POWERSUSPEND
#include <linux/powersuspend.h>
#else
#include <linux/lcd_notify.h>
#endif
#include <linux/hrtimer.h>

#ifdef CONFIG_FB_MSM_MDSS_KCAL_CTRL
#include "../../video/msm/mdss/mdss_mdp_kcal_ctrl.h"
#endif

/* uncomment since no touchscreen defines android touch, do that here */
//#define ANDROID_TOUCH_DECLARED

/* Version, author, desc, etc */
#define DRIVER_AUTHOR "Dennis Rassmann <showp1984@gmail.com>"
#define DRIVER_DESCRIPTION "Sweep2wake for almost any device"
#define DRIVER_VERSION "1.5"
#define LOGTAG "[sweep2wake]: "

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPLv2");

#define KCAL_DOWN	1
#define KCAL_UP		2
#define KCAL_ADJUST	73

#ifdef CONFIG_MACH_MSM8974_HAMMERHEAD
/* Hammerhead aka Nexus 5 */
#define S2W_Y_MAX               1920
#define S2W_X_MAX               1080
#define S2W_Y_LIMIT             S2W_Y_MAX-130
#define S2W_X_B1                400
#define S2W_X_B2                700
#define S2W_X_FINAL             250
#elif defined(CONFIG_MACH_APQ8064_MAKO)
/* Mako aka Nexus 4 */
#define S2W_Y_LIMIT             2350
#define S2W_X_MAX               1540
#define S2W_X_B1                500
#define S2W_X_B2                1000
#define S2W_X_FINAL             300
#elif defined(CONFIG_MACH_APQ8064_FLO)
/* Flo/Deb aka Nexus 7 2013 */
#define S2W_Y_MAX               2240
#define S2W_X_MAX               1344
#define S2W_Y_LIMIT             S2W_Y_MAX-110
#define S2W_X_B1                500
#define S2W_X_B2                700
#define S2W_X_FINAL             450
#else
/* defaults */
#define S2W_Y_MAX               1920
#define S2W_X_MAX               1080
#define S2W_Y_LIMIT             S2W_Y_MAX-130
#define S2W_X_FINAL             250

/* Right -> Left */
#define S2W_X_B0				250
#define S2W_X_B1				S2W_X_B0+150
#define S2W_X_B2				S2W_X_B0+450

/* Left -> Right */
#define S2W_X_B3				S2W_X_B0+130
#define S2W_X_B4				S2W_X_MAX-400
#define S2W_X_B5				S2W_X_MAX-S2W_X_B0
#endif

/* Resources */
int s2w_switch = 0;
int s2d_switch = 0;
static int s2w_debug = 0;
static int s2w_pwrkey_dur = 60;
static int touch_x = 0, touch_y = 0;
static bool touch_x_called = false, touch_y_called = false;
static bool scr_suspended = false, exec_count = true;
static bool scr_on_touch = false, barrier[2] = {false, false};
static bool r_barrier[2] = {false, false};
#ifndef CONFIG_POWERSUSPEND
static struct notifier_block s2w_lcd_notif;
#endif
static struct input_dev * sweep2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct workqueue_struct *s2w_input_wq;
static struct work_struct s2w_input_work;

#ifdef CONFIG_FB_MSM_MDSS_KCAL_CTRL
static void kcal_send_sweep(int send);
#endif

/* Read cmdline for s2w */
static int __init read_s2w_cmdline(char *s2w)
{
	if (strcmp(s2w, "2") == 0) {
		pr_info("[cmdline_s2w]: Sweep2Sleep enabled. | s2w='%s'\n", s2w);
		s2w_switch = 2;
	} else if (strcmp(s2w, "0") == 0) {
		pr_info("[cmdline_s2w]: Sweep2Wake disabled. | s2w='%s'\n", s2w);
		s2w_switch = 0;
	} else {
		pr_info("[cmdline_s2w]: No valid input found. Going with default: | s2w='%u'\n", s2w_switch);
	}
	return 1;
}
__setup("s2w=", read_s2w_cmdline);

/* PowerKey work func */
static void sweep2wake_presspwr(struct work_struct * sweep2wake_presspwr_work) {
	if (!mutex_trylock(&pwrkeyworklock))
                return;
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(s2w_pwrkey_dur);
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(s2w_pwrkey_dur);
        mutex_unlock(&pwrkeyworklock);
	return;
}
static DECLARE_WORK(sweep2wake_presspwr_work, sweep2wake_presspwr);

/* PowerKey trigger */
static void sweep2wake_pwrtrigger(void) {
	schedule_work(&sweep2wake_presspwr_work);
        return;
}

/* reset on finger release */
static void sweep2wake_reset(void) {
	exec_count = true;
	barrier[0] = false;
	barrier[1] = false;
	r_barrier[0] = false;
	r_barrier[1] = false;
	scr_on_touch = false;
}

/* Sweep2wake main function */
static void detect_sweep2wake(int x, int y)
{
	int prevx = 0, nextx = 0;
	int r_prevx = 0, r_nextx = 0;

	if (s2w_debug)
		pr_info(LOGTAG"x: %d, y: %d\n", x, y);

	//right->left
	if ((scr_suspended == false) && ((s2w_switch > 0) || (s2d_switch == 1))) {
		scr_on_touch=true;
		prevx = (S2W_X_MAX - S2W_X_FINAL);
		nextx = S2W_X_B2;
		if ((barrier[0] == true) ||
		   ((x < prevx) &&
		    (x > nextx) &&
		    (y > S2W_Y_LIMIT))) {
			prevx = nextx;
			nextx = S2W_X_B1;
			barrier[0] = true;
			if ((barrier[1] == true) ||
			   ((x < prevx) &&
			    (x > nextx) &&
			    (y > S2W_Y_LIMIT))) {
				prevx = nextx;
				barrier[1] = true;
				if ((x < prevx) &&
				    (y > S2W_Y_LIMIT)) {
					if (x < S2W_X_FINAL) {
						if (exec_count) {
							pr_info(LOGTAG"OFF\n");
							if (s2d_switch == 1)
#ifdef CONFIG_FB_MSM_MDSS_KCAL_CTRL
								kcal_send_sweep(KCAL_DOWN);
#else
								pr_info(LOGTAG"sweep2dim not available\n");
#endif
							else
								sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
		r_prevx = S2W_X_B0;
		r_nextx = S2W_X_B3;
		if ((r_barrier[0] == true) ||
		   ((x > r_prevx) &&
			(x < r_nextx) &&
			(y > S2W_Y_LIMIT))) {
			r_prevx = r_nextx;
			r_nextx = S2W_X_B4;
			r_barrier[0] = true;
			if ((r_barrier[1] == true) ||
			   ((x > r_prevx) &&
				(x < r_nextx) &&
				(y > S2W_Y_LIMIT))) {
				r_prevx = r_nextx;
				r_barrier[1] = true;
				if ((x > r_prevx) &&
					(y > S2W_Y_LIMIT)) {
					if (x > S2W_X_B5) {
						if (exec_count) {
							pr_info(LOGTAG"OFF\n");
							if (s2d_switch == 1)
#ifdef CONFIG_FB_MSM_MDSS_KCAL_CTRL
								kcal_send_sweep(KCAL_UP);
#else
								pr_info(LOGTAG"sweep2dim not available\n");
#endif
							else
								sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
	}
}

#ifdef CONFIG_FB_MSM_MDSS_KCAL_CTRL
static void kcal_send_sweep(int send)
{
	int kcal_r, kcal_g, kcal_b;

	mdss_mdp_pp_kcal_update(kcal_r = NUM_QLUT, kcal_g = NUM_QLUT, kcal_b =  NUM_QLUT);
	
	switch (send) {
		case KCAL_DOWN:
			kcal_r -= KCAL_ADJUST;
			kcal_g -= KCAL_ADJUST;
			kcal_b -= KCAL_ADJUST;
			kcal_r = (kcal_r < 35) ? 35 : kcal_r;
			kcal_g = (kcal_g < 35) ? 35 : kcal_g;
			kcal_b = (kcal_b < 35) ? 35 : kcal_b;
			break;
		case KCAL_UP:
			kcal_r += KCAL_ADJUST;
			kcal_g += KCAL_ADJUST;
			kcal_b += KCAL_ADJUST;
			kcal_r = (kcal_r > 255) ? 255 : kcal_r;
			kcal_g = (kcal_g > 255) ? 255 : kcal_g;
			kcal_b = (kcal_b > 255) ? 255 : kcal_b;
			break;
	}

	mdss_mdp_pp_kcal_update(kcal_r, kcal_g, kcal_b);
}
#endif

static void s2w_input_callback(struct work_struct *unused) {

	if (s2w_switch || s2d_switch)
		detect_sweep2wake(touch_x, touch_y);

	return;
}

static void s2w_input_event(struct input_handle *handle, unsigned int type,
				unsigned int code, int value)
{
	if ((!s2w_switch && !s2d_switch) ||
				((scr_suspended) && (s2w_switch > 1)))
		return;

	if (code == ABS_MT_SLOT) {
		sweep2wake_reset();
		return;
	}

	if (code == ABS_MT_TRACKING_ID && value == -1) {
		sweep2wake_reset();
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		touch_x = value;
		touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		touch_y = value;
		touch_y_called = true;
	}

	if (touch_x_called && touch_y_called) {
		touch_x_called = false;
		touch_y_called = false;
		queue_work_on(0, s2w_input_wq, &s2w_input_work);
	}
}

static int input_dev_filter(struct input_dev *dev) {
	if (strstr(dev->name, "touch") ||
		strstr(dev->name, "synaptics-rmi-ts")) {
		return 0;
	} else {
		return 1;
	}
}

static int s2w_input_connect(struct input_handler *handler,
				struct input_dev *dev, const struct input_device_id *id) {
	struct input_handle *handle;
	int error;

	if (input_dev_filter(dev))
		return -ENODEV;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "s2w";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void s2w_input_disconnect(struct input_handle *handle) {
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id s2w_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler s2w_input_handler = {
	.event		= s2w_input_event,
	.connect	= s2w_input_connect,
	.disconnect	= s2w_input_disconnect,
	.name		= "s2w_inputreq",
	.id_table	= s2w_ids,
};

#ifdef CONFIG_POWERSUSPEND
static void s2w_power_suspend(struct power_suspend *h) {
	scr_suspended = true;
}

static void s2w_power_resume(struct power_suspend *h) {
	scr_suspended = false;
}

static struct power_suspend s2w_power_suspend_handler = {
	.suspend = s2w_power_suspend,
	.resume = s2w_power_resume,
};
#else
static int lcd_notifier_callback(struct notifier_block *this,
								unsigned long event, void *data)
{
	switch (event) {
	case LCD_EVENT_ON_END:
		scr_suspended = false;
		break;
	case LCD_EVENT_OFF_END:
		scr_suspended = true;
		break;
	default:
		break;
	}

	return 0;
}
#endif

/*
 * SYSFS stuff below here
 */
static ssize_t s2w_sweep2wake_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", s2w_switch);

	return count;
}

static ssize_t s2w_sweep2wake_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] >= '0' && buf[0] <= '2' && buf[0] != '1' && buf[1] == '\n')
	    if ((s2w_switch != buf[0] - '0') && (s2d_switch != 1))
			s2w_switch = buf[0] - '0';

	return count;
}

static DEVICE_ATTR(sweep2wake, (S_IWUSR|S_IRUGO),
	s2w_sweep2wake_show, s2w_sweep2wake_dump);

static ssize_t s2w_sweep2dim_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%d\n", s2d_switch);

	return count;
}

static ssize_t s2w_sweep2dim_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] >= '0' && buf[0] <= '1' && buf[1] == '\n')
		if ((s2d_switch != buf[0] - '0') && (s2w_switch != '2'))
			s2d_switch = buf[0] - '0';

	return count;
}

static DEVICE_ATTR(sweep2dim, (S_IWUSR|S_IRUGO),
	s2w_sweep2dim_show, s2w_sweep2dim_dump);

static ssize_t s2w_debug_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", s2w_debug);
}

static ssize_t s2w_debug_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	if (buf[0] >= '0' && buf[0] <= '1' && buf[1] == '\n')
		if (s2w_debug != buf[0] - '0')
			s2w_debug = buf[0] - '0';

	return count;
}

static DEVICE_ATTR(sweep2wake_debug, (S_IWUSR|S_IRUGO),
	s2w_debug_show, s2w_debug_dump);

static ssize_t s2w_pwrkey_dur_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", s2w_pwrkey_dur);
}

static ssize_t s2w_pwrkey_dur_dump(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val, err;

	err = kstrtoint(buf, 0, &val);
	if (err < 0) {
		printk(KERN_ERR "%s: Failed to convert value.\n", __func__);
		return -EINVAL;
	}

	if ((!val) || (val > 3000)) {
		printk(KERN_ERR "%s: value is out of range.\n", __func__);
		return -EINVAL;
	} else
		s2w_pwrkey_dur = val;

	return count;
}

static DEVICE_ATTR(sweep2wake_pwrkey_dur, (S_IWUSR|S_IRUGO),
	s2w_pwrkey_dur_show, s2w_pwrkey_dur_dump);

static ssize_t s2w_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t count = 0;

	count += sprintf(buf, "%s\n", DRIVER_VERSION);

	return count;
}

static DEVICE_ATTR(sweep2wake_version, (S_IWUSR|S_IRUGO),
	s2w_version_show, NULL);

/*
 * INIT / EXIT stuff below here
 */
#ifdef ANDROID_TOUCH_DECLARED
extern struct kobject *android_touch_kobj;
#else
struct kobject *android_touch_kobj;
EXPORT_SYMBOL_GPL(android_touch_kobj);
#endif
static int __init sweep2wake_init(void)
{
	int rc = 0;

	sweep2wake_pwrdev = input_allocate_device();
	if (!sweep2wake_pwrdev) {
		pr_err("Can't allocate suspend autotest power button\n");
		goto err_alloc_dev;
	}

	input_set_capability(sweep2wake_pwrdev, EV_KEY, KEY_POWER);
	sweep2wake_pwrdev->name = "s2w_pwrkey";
	sweep2wake_pwrdev->phys = "s2w_pwrkey/input0";

	rc = input_register_device(sweep2wake_pwrdev);
	if (rc) {
		pr_err("%s: input_register_device err=%d\n", __func__, rc);
		goto err_input_dev;
	}

	s2w_input_wq = create_workqueue("s2wiwq");
	if (!s2w_input_wq) {
		pr_err("%s: Failed to create s2wiwq workqueue\n", __func__);
		return -EFAULT;
	}
	INIT_WORK(&s2w_input_work, s2w_input_callback);
	rc = input_register_handler(&s2w_input_handler);
	if (rc)
		pr_err("%s: Failed to register s2w_input_handler\n", __func__);

#ifdef CONFIG_POWERSUSPEND
	register_power_suspend(&s2w_power_suspend_handler);
#else
	s2w_lcd_notif.notifier_call = lcd_notifier_callback;
	if (lcd_register_client(&s2w_lcd_notif) != 0) {
		pr_err("%s: Failed to register lcd callback\n", __func__);
	}
#endif

#ifndef ANDROID_TOUCH_DECLARED
	android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
	if (android_touch_kobj == NULL) {
		pr_warn("%s: android_touch_kobj create_and_add failed\n", __func__);
	}
#endif
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2wake.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2dim.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2dim\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2wake_debug.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake_debug\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2wake_pwrkey_dur.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake_pwrkey_dur\n", __func__);
	}
	rc = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2wake_version.attr);
	if (rc) {
		pr_warn("%s: sysfs_create_file failed for sweep2wake_version\n", __func__);
	}

err_input_dev:
	input_free_device(sweep2wake_pwrdev);
err_alloc_dev:
	pr_info(LOGTAG"%s done\n", __func__);

	return 0;
}

static void __exit sweep2wake_exit(void)
{
#ifndef ANDROID_TOUCH_DECLARED
	kobject_del(android_touch_kobj);
#endif
#ifndef CONFIG_POWERSUSPEND
	lcd_unregister_client(&s2w_lcd_notif);
#endif
	input_unregister_handler(&s2w_input_handler);
	destroy_workqueue(s2w_input_wq);
	input_unregister_device(sweep2wake_pwrdev);
	input_free_device(sweep2wake_pwrdev);
	return;
}

late_initcall(sweep2wake_init);
module_exit(sweep2wake_exit);
