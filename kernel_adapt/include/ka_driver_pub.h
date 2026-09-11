/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Description:
 * Author: huawei
 * Create: 2024-11-11
 */

#ifndef KA_DRIVER_PUB_H
#define KA_DRIVER_PUB_H

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/acpi.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#include <linux/gpio/consumer.h>
#endif
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/uuid.h>
#include <linux/notifier.h>
#include <linux/mdev.h>
#include <linux/vfio.h>
#include <linux/workqueue.h>

#include "ka_macro_pub.h"
#include "ka_common_pub.h"

typedef struct clk ka_clk_t;
typedef struct gendisk ka_gendisk_t;
typedef struct acpi_object_list ka_acpi_object_list_t;
typedef struct acpi_buffer ka_acpi_buffer_t;
typedef struct fwnode_handle ka_fwnode_handle_t;

typedef struct miscdevice ka_miscdevice_t;
#define KA_MISC_DYNAMIC_MINOR MISC_DYNAMIC_MINOR
#define ka_driver_init_md_minor(md_minor) \
    .minor = md_minor,
#define ka_driver_init_md_name(md_name) \
    .name = md_name,
#define ka_driver_init_md_fops(md_fops) \
    .fops = md_fops,

typedef struct dmi_system_id ka_dmi_system_id_t;
typedef struct dmi_device ka_dmi_device_t;
typedef struct gpio_desc ka_gpio_desc_t;
typedef struct i2c_adapter ka_i2c_adapter_t;
typedef struct i2c_driver ka_i2c_driver_t;
typedef struct i2c_msg ka_i2c_msg_t;
typedef struct i2c_client ka_i2c_client_t;
typedef union i2c_smbus_data ka_i2c_smbus_data_t;
typedef struct iova ka_iova_t;
typedef struct iova_fq ka_iova_fq_t;
typedef struct property ka_property_t;
typedef struct of_phandle_args ka_of_phandle_args_t;
typedef struct of_device_id ka_of_device_id_t;
typedef struct vc_data ka_vc_data_t;
typedef struct mdev_device ka_mdev_device_t;
typedef struct mdev_parent_ops ka_mdev_parent_ops_t;
typedef struct vfio_info_cap ka_vfio_info_cap_t;
typedef struct vfio_info_cap_header ka_vfio_info_cap_header_t;
typedef struct vfio_irq_set ka_vfio_irq_set_t;
typedef struct device_attribute ka_device_attribute_t;
typedef struct iova_domain ka_iova_domain_t;
typedef enum gpiod_flags ka_gpiod_flags_t;
typedef char* (*ka_class_devnode)(ka_device_t *dev, umode_t *mode);

#define KA_IOVA_FQ_SIZE IOVA_FQ_SIZE

#define KA_MINORBITS    MINORBITS
#define KA_MINORMASK    MINORMASK

#define KA_DRIVER_MAJOR(dev)       MAJOR(dev)
#define KA_DRIVER_MINOR(dev)       MINOR(dev)
#define KA_DRIVER_MKDEV(ma, mi)    MKDEV(ma, mi)

#define KA_DRIVER_ALIGN(x, a)                  ALIGN(x, a)
#define KA_DRIVER_IS_ALIGNED(x, a)		       IS_ALIGNED(x, a)

#define KA_DRIVER_DEVICE_ATTR(_name, _mode, _show, _store) DEVICE_ATTR(_name, _mode, _show, _store)

#define ka_driver_clk_disable(clk) clk_disable(clk)
#define ka_driver_clk_enable(clk) clk_enable(clk)
#define ka_driver_clk_get_parent(clk) clk_get_parent(clk)
#define ka_driver_clk_get_rate(clk) clk_get_rate(clk)
int ka_driver_get_acpi_disabled(void);
#define ka_driver_acpi_evaluate_object(handle, pathname, external_params, return_buffer) acpi_evaluate_object(handle, pathname, external_params, return_buffer)
#define ka_driver_acpi_evaluate_object_typed(handle, pathname, external_params, return_buffer, return_type) acpi_evaluate_object_typed(handle, pathname, external_params, return_buffer, return_type)
#define ka_driver_acpi_get_devices(HID, user_function, context, return_value) acpi_get_devices(HID, user_function, context, return_value)
#define ka_driver_is_acpi_device_node(fwnode) is_acpi_device_node(fwnode)
#define ka_driver_dev_driver_string(dev) dev_driver_string(dev)
#define ka_driver_misc_deregister(misc) misc_deregister(misc)
#define ka_driver_misc_register(misc) misc_register(misc)

#define ka_driver_get_random_bytes(buf, len) get_random_bytes(buf, len)
#define ka_driver_dmi_check_system(list) dmi_check_system(list)
#define ka_driver_dmi_find_device(type, name, from) dmi_find_device(type, name, from)
#define ka_driver_dmi_match(f, str) dmi_match(f, str)
#define ka_driver_devm_gpiod_get_optional(dev, con_id, flags) devm_gpiod_get_optional(dev, con_id, flags)
#define ka_driver_i2c_del_adapter(adap) i2c_del_adapter(adap)
#define ka_driver_i2c_del_driver(driver) i2c_del_driver(driver)
#define ka_driver_i2c_get_adapter(nr) i2c_get_adapter(nr)
#define ka_driver_i2c_put_adapter(adap) i2c_put_adapter(adap)
#define ka_driver_i2c_register_driver(owner, driver) i2c_register_driver(owner, driver)
#define ka_driver_i2c_transfer(adap, msgs, num) i2c_transfer(adap, msgs, num)
#define ka_driver_i2c_transfer_buffer_flags(client, buf, count, flags) i2c_transfer_buffer_flags(client, buf, count, flags)
#define ka_driver_i2c_verify_client(dev) i2c_verify_client(dev) 
#define ka_driver_i2c_smbus_read_word_data(client, command) i2c_smbus_read_word_data(client, command)
#define ka_driver_i2c_smbus_write_word_data(client, command, value) i2c_smbus_write_word_data(client, command, value)
#define ka_driver_i2c_smbus_xfer(adapter, addr, flags, read_write, command, protocol, data) i2c_smbus_xfer(adapter, addr, flags, read_write, command, protocol, data)
#define ka_driver_of_get_address(dev, index, size, flags) of_get_address(dev, index, size, flags)
#define ka_driver_of_device_is_compatible(device, compat) of_device_is_compatible(device, compat)
#define ka_driver_of_find_compatible_node(from, type, compatible) of_find_compatible_node(from, type, compatible)
#define ka_driver_of_find_property(np, name, lenp) of_find_property(np, name, lenp)
#define ka_driver_of_get_next_available_child(node, prev) of_get_next_available_child(node, prev)
#define ka_driver_of_get_next_child(node, prev) of_get_next_child(node, prev)
#define ka_driver_of_parse_phandle(np, phandle_name, index) of_parse_phandle(np, phandle_name, index)
#define ka_driver_of_parse_phandle_with_args(np, list_name, cells_name, index, out_args) of_parse_phandle_with_args(np, list_name, cells_name, index, out_args)
#define ka_driver_of_parse_phandle_with_args_map(np, list_name, stem_name, index, out_args) of_parse_phandle_with_args_map(np, list_name, stem_name, index, out_args)
#define ka_driver_of_parse_phandle_with_fixed_args(np, list_name, cell_count, index, out_args) of_parse_phandle_with_fixed_args(np, list_name, cell_count, index, out_args)
#define ka_driver_of_match_device(matches, dev) of_match_device(matches, dev)

#ifndef __cplusplus
ka_class_t *ka_driver_class_create(ka_module_t *owner, const char *name);

#ifndef EMU_ST
int ka_driver_class_set_devnode(ka_class_t *cls, ka_class_devnode devnode);
#endif

#define ka_driver_class_destroy(cls) class_destroy(cls)
#define ka_driver_device_create device_create
#define ka_driver_device_destroy(class, devt) device_destroy(class, devt)
#endif

#define ka_driver_of_node_put(node) of_node_put(node)
#define ka_driver_mdev_dev(mdev) mdev_dev(mdev)
#define ka_driver_mdev_from_dev(dev) mdev_from_dev(dev)
#define ka_driver_mdev_get_drvdata(mdev) mdev_get_drvdata(mdev)
#define ka_driver_mdev_parent_dev(mdev) mdev_parent_dev(mdev)
#define ka_driver_mdev_register_device(dev, ops) mdev_register_device(dev, ops)
#define ka_driver_mdev_set_drvdata(mdev, data) mdev_set_drvdata(mdev, data)
#define ka_driver_mdev_unregister_device(dev) mdev_unregister_device(dev)

#define ka_driver_vfio_info_add_capability(caps, cap, size) vfio_info_add_capability(caps, cap, size)
#define ka_driver_vfio_info_cap_shift(caps, offset) vfio_info_cap_shift(caps, offset)
#define ka_driver_vfio_pin_pages(dev, user_pfn, npage, prot, phys_pfn) vfio_pin_pages(dev, user_pfn, npage, prot, phys_pfn)
#define ka_driver_vfio_register_notifier(dev, type, events, nb) vfio_register_notifier(dev, type, events, nb)
#define ka_driver_vfio_set_irqs_validate_and_prepare(hdr, num_irqs, max_irq_type, data_size) vfio_set_irqs_validate_and_prepare(hdr, num_irqs, max_irq_type, data_size)
#define ka_driver_vfio_unpin_pages(dev, user_pfn, npage) vfio_unpin_pages(dev, user_pfn, npage)
#define ka_driver_vfio_unregister_notifier(dev, type, nb) vfio_unregister_notifier(dev, type, nb)
#define ka_driver_dev_to_node(dev) dev_to_node(dev)


#endif
