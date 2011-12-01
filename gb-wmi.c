#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/acpi.h>

#define GB_WMI_EVENT_GUID "ABBC0F72-8EA1-11D1-00A0-C90629100000"

MODULE_AUTHOR("Igor Zavorotkin <ivzave@gmail.com");
MODULE_DESCRIPTION("Gigabyte Notebook WMI driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("wmi:"GB_WMI_EVENT_GUID);

static const struct key_entry gb_wmi_keymap[] = {
	{ KE_KEY, 0xcf, { KEY_FRONT } },
	{ KE_END, 0 }
};

static struct input_dev* gb_wmi_input_dev;

static int gb_wmi_input_setup()
{
	int err;
	gb_wmi_input_dev = input_allocate_device();
	if (!gb_wmi_input_dev)
		return -ENOMEM;
	
	gb_wmi_input_dev->name = "GB WMI input device";
	gb_wmi_input_dev->phys = "wmi/input0";
	gb_wmi_input_dev->id.bustype = BUS_HOST;
	
	err = sparse_keymap_setup(gb_wmi_input_dev, gb_wmi_keymap, NULL);
	if (err)
		goto err_free_dev;

	err = input_register_device(gb_wmi_input_dev);

	if (err)
		goto err_free_keymap;

	return 0;

err_free_keymap:
	sparse_keymap_free(gb_wmi_input_dev);
err_free_dev:
	input_free_device(gb_wmi_input_dev);
	
	return err;
}

static void gb_wmi_notify(u32 value, void* context)
{
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object* obj;
	acpi_status status;
	int code;
	
	status = wmi_get_event_data(value, &response);
	if (status != AE_OK) {
		pr_err("Bad event status 0x%x\n", status);
		return;
	}
	
	obj = (union acpi_object*)response.pointer;
	if (!obj) {
		pr_info("Could not get response pointer\n");
		return;
	}

	if (obj->type == ACPI_TYPE_BUFFER) {
		int eventcode = obj->buffer.pointer[0];
		struct key_entry* key = sparse_keymap_entry_from_scancode(gb_wmi_input_dev, eventcode);
		pr_info("Buffer received: %x %x \n", obj->buffer.length, obj->buffer.pointer[0], obj->buffer.pointer[1]);
		if (key)
			sparse_keymap_report_entry(gb_wmi_input_dev, key, 1, true);
	}
	else
		pr_info("Unknown event\n");
	
	
	kfree(obj);
}

static int __init gb_wmi_init(void)
{
	int err;
	
	if (!wmi_has_guid(GB_WMI_EVENT_GUID)) {
		pr_err("This machine doesn't have GB input through WMI\n");
		return -ENODEV;
	}
	err = gb_wmi_input_setup();
	if (err)
		return err;

	err = wmi_install_notify_handler(GB_WMI_EVENT_GUID, gb_wmi_notify, NULL);
	if (ACPI_FAILURE(err)) {
		return -ENODEV;
	}
	pr_info("GB WMI driver loaded\n");
	return 0;
}

static void __exit gb_wmi_exit(void)
{
	if (wmi_has_guid(GB_WMI_EVENT_GUID)) {
		wmi_remove_notify_handler(GB_WMI_EVENT_GUID);
		sparse_keymap_free(gb_wmi_input_dev);
		input_unregister_device(gb_wmi_input_dev);
	}
}

module_init(gb_wmi_init);
module_exit(gb_wmi_exit);
