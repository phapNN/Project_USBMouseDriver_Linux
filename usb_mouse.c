#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define DEVICE_NAME "my_mouse"
#define CLASS_NAME  "mouse_class"
#define BUF_SIZE    1024

static struct input_handler my_mouse_handler;
static struct input_handle *global_handle = NULL;

static int major_number;
static struct class* mouse_class  = NULL;
static struct device* mouse_device = NULL;

static char mouse_buf[BUF_SIZE];
static int mouse_buf_len = 0;
static DEFINE_MUTEX(mouse_mutex);
static int abs_x = 0, abs_y = 0;
static int left_clicks = 0, right_clicks = 0, wheel_clicks = 0;

//
// Device file read(): copy buffer JSON to userspace
//
static ssize_t dev_read(struct file *filep, char *user_buffer, size_t len, loff_t *offset) {
    ssize_t ret;

    mutex_lock(&mouse_mutex);

    if (*offset >= mouse_buf_len) {
        mutex_unlock(&mouse_mutex);
        return 0;
    }

    if (len > mouse_buf_len - *offset)
        len = mouse_buf_len - *offset;

    if (copy_to_user(user_buffer, mouse_buf + *offset, len)) {
        mutex_unlock(&mouse_mutex);
        return -EFAULT;
    }

    *offset += len;
    ret = len;

    mutex_unlock(&mouse_mutex);
    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};

//
// Ghi JSON data vào buffer khi có sự kiện chuột
//
static void my_mouse_event(struct input_handle *handle, unsigned int type, unsigned int code, int value) {
    static int rel_x = 0, rel_y = 0;
    char pos[256]; // Increased buffer size

    if (type == EV_REL) {
        switch (code) {
            case REL_X:
                rel_x = value;
                abs_x += value;
                break;
            case REL_Y:
                rel_y = value;
                abs_y += value;
                break;
            case REL_WHEEL:
                wheel_clicks += value;
                break;
        }
    } else if (type == EV_KEY) {
        switch (code) {
            case BTN_LEFT:
                if (value == 1) left_clicks++;
                break;
            case BTN_RIGHT:
                if (value == 1) right_clicks++;
                break;
        }
    }

    abs_x = max(0, min(abs_x, 1920 - 1));
    abs_y = max(0, min(abs_y, 1080 - 1));

    if (type == EV_SYN) {
        snprintf(pos, sizeof(pos), "{\"x\": %d, \"y\": %d, \"abs_x\": %d, \"abs_y\": %d, \"left_clicks\": %d, \"right_clicks\": %d, \"wheel_clicks\": %d}\n",
                 rel_x, rel_y, abs_x, abs_y, left_clicks, right_clicks, wheel_clicks);

        mutex_lock(&mouse_mutex);
        mouse_buf_len = snprintf(mouse_buf, BUF_SIZE, "%s", pos);
        mutex_unlock(&mouse_mutex);

        printk(KERN_INFO "📤 Mouse JSON: %s", pos);
    }
}

//
// Gắn handler vào thiết bị chuột
//
static int my_mouse_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id) {
    struct input_handle *handle;

    // Chỉ nhận thiết bị có REL_X và REL_Y (chuột di chuyển)
    if (!test_bit(EV_REL, dev->evbit) ||
        !test_bit(REL_X, dev->relbit) ||
        !test_bit(REL_Y, dev->relbit)) {
        return -ENODEV;
    }

    // 👉 Thêm điều kiện: chỉ nhận thiết bị có tên chứa "Mouse"
    if (strnstr(dev->name, "Mouse", strlen(dev->name)) == NULL) {
        printk(KERN_INFO "❌ Not Usb mouse: %s\n", dev->name);
        return -ENODEV;
    }

    // Cấp phát bộ nhớ và gắn handle
    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "my_mouse_handle";

    // Đăng ký handle
    if (input_register_handle(handle)) {
        kfree(handle);
        return -EINVAL;
    }

    // Mở thiết bị
    if (input_open_device(handle)) {
        input_unregister_handle(handle);
        kfree(handle);
        return -EINVAL;
    }

    global_handle = handle;

    printk(KERN_INFO "🖱️  Mouse connected: %s\n", dev->name);
    return 0;
}

static void my_mouse_disconnect(struct input_handle *handle) {
    printk(KERN_INFO "🛑 Mouse disconnected\n");
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

static const struct input_device_id my_mouse_ids[] = {
    {
        .driver_info = 1,
    },
    {}
};

static struct input_handler my_mouse_handler = {
    .event      = my_mouse_event,
    .connect    = my_mouse_connect,
    .disconnect = my_mouse_disconnect,
    .name       = "my_mouse_handler",
    .id_table   = my_mouse_ids,
};

//
// Khởi tạo module: đăng ký thiết bị và input handler
//
static int __init my_mouse_init(void) {
    printk(KERN_INFO "🔌 Loading Mouse Logger v2...\n");

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "❌ Failed to register a major number\n");
        return major_number;
    }

    mouse_class = class_create(CLASS_NAME);
    if (IS_ERR(mouse_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "❌ Failed to register device class\n");
        return PTR_ERR(mouse_class);
    }

    mouse_device = device_create(mouse_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(mouse_device)) {
        class_destroy(mouse_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "❌ Failed to create device\n");
        return PTR_ERR(mouse_device);
    }

    mutex_init(&mouse_mutex);
    return input_register_handler(&my_mouse_handler);
}

//
// Gỡ module: cleanup
//
static void __exit my_mouse_exit(void) {
    input_unregister_handler(&my_mouse_handler);
    device_destroy(mouse_class, MKDEV(major_number, 0));
    class_destroy(mouse_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    mutex_destroy(&mouse_mutex);

    printk(KERN_INFO "👋 Unloaded Mouse Logger v2\n");
}

module_init(my_mouse_init);
module_exit(my_mouse_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PhapHieu");
MODULE_DESCRIPTION("Mouse Logger Kernel Module with /dev and JSON output");
