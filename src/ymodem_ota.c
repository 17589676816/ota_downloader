/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-01-30     armink       the first version
 * 2018-08-27     Murphy       update log
 */

#include <rtthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <finsh.h>
#include <fal.h>
#include <ymodem.h>

#define DBG_ENABLE
#define DBG_SECTION_NAME               "ymodem"
#ifdef OTA_DOWNLOADER_DEBUG
#define DBG_LEVEL                      DBG_LOG
#else
#define DBG_LEVEL                      DBG_INFO
#endif
#define DBG_COLOR
#include <rtdbg.h>

#ifdef PKG_USING_YMODEM_OTA

#define DEFAULT_DOWNLOAD_PART "download"

static size_t update_file_total_size, update_file_cur_size;
static const struct fal_partition * dl_part = RT_NULL;
static uint8_t enable_output_log = 0;

static enum rym_code ymodem_on_begin(struct rym_ctx *ctx, rt_uint8_t *buf, rt_size_t len)
{
    char *file_name, *file_size;

    /* calculate and store file size */
    file_name = (char *)&buf[0];
    file_size = (char *)&buf[rt_strlen(file_name) + 1];
    update_file_total_size = atol(file_size);
    if (enable_output_log) {rt_kprintf("Ymodem file_size:%d\n", update_file_total_size);}

    update_file_cur_size = 0;

    /* Get download partition information and erase download partition data */
    if (update_file_total_size > dl_part->len)
    {
        if (enable_output_log) {LOG_E("Firmware is too large! File size (%d), '%s' partition size (%d)", update_file_total_size, dl_part->name, dl_part->len);}
        return RYM_CODE_CAN;
    }

    if (enable_output_log) {LOG_I("Start erase. Size (%d)", update_file_total_size);}

    /* erase DL section */
    if (fal_partition_erase(dl_part, 0, update_file_total_size) < 0)
    {
        if (enable_output_log) {LOG_E("Firmware download failed! Partition (%s) erase error!", dl_part->name);}
        return RYM_CODE_CAN;
    }

    return RYM_CODE_ACK;
}

static enum rym_code ymodem_on_data(struct rym_ctx *ctx, rt_uint8_t *buf, rt_size_t len)
{
    /* write data of application to DL partition  */
    if (fal_partition_write(dl_part, update_file_cur_size, buf, len) < 0)
    {
        if (enable_output_log) {LOG_E("Firmware download failed! Partition (%s) write data error!", dl_part->name);}
        return RYM_CODE_CAN;
    }

    update_file_cur_size += len;

    return RYM_CODE_ACK;
}

void ymodem_ota(uint8_t argc, char **argv)
{
    struct rym_ctx rctx;
    const char str_usage[] = "Usage: ymodem_ota -p <partiton name> -t <device name>.\n";
    int i;
    char* recv_partition = DEFAULT_DOWNLOAD_PART;
    rt_device_t dev = rt_console_get_device();
    enable_output_log = 0;
	rt_kprintf("argc = %d\n",argc);
    
    for (i=1; i<argc;)
    {
		rt_kprintf("argv[%d] = %s\n",i,argv[i]);
        /* change default partition to save firmware */
        if (!strcmp(argv[i], "-p"))
        {
            if (argc <= (i+1))
            {
                rt_kprintf("%s", str_usage);
                return;
            }
            recv_partition = argv[i+1];
            i += 2;
        }
        /* change default device to transfer */
        else if (!strcmp(argv[i], "-t"))
        {
            if (argc <= (i+1))
            {
                rt_kprintf("%s", str_usage);
                return;
            }
			rt_kprintf("argv[%d + 1] = %s\n",i,argv[i+1]);
            dev = rt_device_find(argv[i+1]);
            if (dev == RT_NULL)
            {
                rt_kprintf("Device (%s) find error!\n", argv[i+1]);
                return;
            }
            i += 2;
        }
        /* NOT supply parameter */
        else
        {
            rt_kprintf("%s", str_usage);
            return;
        }
    }
    if ((dl_part = fal_partition_find(recv_partition)) == RT_NULL)
    {
        rt_kprintf("Partition (%s) find error!\n", recv_partition);
        return;
    }
    if (dev != rt_console_get_device()) {enable_output_log = 1;}
    rt_kprintf("Save firmware on \"%s\" partition with device \"%s\".\n", recv_partition, dev->parent.name);
    rt_kprintf("Warning: Ymodem has started! This operator will not recovery.\n");
    rt_kprintf("Please select the ota firmware file and use Ymodem to send.\n");

    if (!rym_recv_on_device(&rctx, dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                            ymodem_on_begin, ymodem_on_data, NULL, RT_TICK_PER_SECOND))
    {
        rt_kprintf("Download firmware to flash success.\n");
        rt_kprintf("System now will restart...\r\n");

        /* wait some time for terminal response finish */
        rt_thread_delay(rt_tick_from_millisecond(200));

        /* Reset the device, Start new firmware */
        extern void rt_hw_cpu_reset(void);
        rt_hw_cpu_reset();
        /* wait some time for terminal response finish */
        rt_thread_delay(rt_tick_from_millisecond(200));
    }
    else
    {
        /* wait some time for terminal response finish */
        rt_thread_delay(RT_TICK_PER_SECOND);
        rt_kprintf("Update firmware fail.\n");
    }

    return;
}
/**
 * msh />ymodem_ota
*/
MSH_CMD_EXPORT(ymodem_ota, Use Y-MODEM to download the firmware);

#endif /* PKG_USING_YMODEM_OTA */
