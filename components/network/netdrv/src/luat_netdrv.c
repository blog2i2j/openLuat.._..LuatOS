#include "luat_base.h"
#include "luat_netdrv.h"
#include "luat_network_adapter.h"
#ifdef __LUATOS__
#include "luat_netdrv_ch390h.h"
#include "luat_netdrv_uart.h"
#endif
#include "luat_mem.h"

#define LUAT_LOG_TAG "netdrv"
#include "luat_log.h"

static luat_netdrv_t* drvs[NW_ADAPTER_QTY];

luat_netdrv_t* luat_netdrv_setup(luat_netdrv_conf_t *conf) {
    if (conf->id < 0 || conf->id >= NW_ADAPTER_QTY) {
        return NULL;
    }
    if (drvs[conf->id] == NULL) {
        // 注册新的设备?
        #ifdef __LUATOS__
        if (conf->impl == 1) { // CH390H
            drvs[conf->id] = luat_netdrv_ch390h_setup(conf);
            return drvs[conf->id];
        }
        if (conf->impl == 16) { // UART
            drvs[conf->id] = luat_netdrv_uart_setup(conf);
            return drvs[conf->id];
        }
        #endif
    }
    else {
        if (drvs[conf->id]->boot) {
            drvs[conf->id]->boot(NULL);
        }
    }
    return NULL;
}

int luat_netdrv_dhcp(int32_t id, int32_t enable) {
    if (id < 0 || id >= NW_ADAPTER_QTY) {
        return -1;
    }
    if (drvs[id] == NULL) {
        return -1;
    }
    if (drvs[id]->dhcp == NULL) {
        LLOGW("该netdrv不支持设置dhcp开关");
        return -1;
    }
    return drvs[id]->dhcp(drvs[id]->userdata, enable);
}

int luat_netdrv_ready(int32_t id) {
    if (drvs[id] == NULL) {
        return -1;
    }
    return drvs[id]->ready(drvs[id]->userdata);
}

int luat_netdrv_register(int32_t id, luat_netdrv_t* drv) {
    if (drvs[id] != NULL) {
        return -1;
    }
    drvs[id] = drv;
    return 0;
}

int luat_netdrv_mac(int32_t id, const char* new, char* old) {
    if (id < 0 || id >= NW_ADAPTER_QTY) {
        return -1;
    }
    if (drvs[id] == NULL || drvs[id]->netif == NULL) {
        return -1;
    }
    memcpy(old, drvs[id]->netif->hwaddr, 6);
    memcpy(drvs[id]->netif->hwaddr, new, 6);
    return 0;
}

void luat_netdrv_stat_inc(luat_netdrv_statics_item_t* stat, size_t len) {
    stat->bytes += len;
    stat->counter ++;
}

luat_netdrv_t* luat_netdrv_get(int id) {
    if (id < 0 || id >= NW_ADAPTER_INDEX_LWIP_NETIF_QTY) {
        return NULL;
    }
    return drvs[id];
}

static char tmpbuff[1024];
void luat_netdrv_print_pkg(const char* tag, uint8_t* buff, size_t len) {
    if (len > 511) {
        len = 511;
    }
    memset(tmpbuff, 0, 1024);
    for (size_t i = 0; i < len; i++)
    {
        sprintf(tmpbuff + i * 2, "%02X", buff[i]);
    }
    LLOGD("%s %s", tag, tmpbuff);
}

#define NAPT_CHKSUM_16BIT_LEN        sizeof(uint16_t)

uint32_t alg_hdr_16bitsum(const uint16_t *buff, uint16_t len)
{
    uint32_t sum = 0;

    uint16_t *pos = (uint16_t *)buff;
    uint16_t remainder_size = len;

    while (remainder_size > 1)
    {
        sum += *pos ++;
        remainder_size -= NAPT_CHKSUM_16BIT_LEN;
    }

    if (remainder_size > 0)
    {
        sum += *(uint8_t*)pos;
    }

    return sum;
}

uint16_t alg_iphdr_chksum(const uint16_t *buff, uint16_t len)
{
    uint32_t sum = alg_hdr_16bitsum(buff, len);

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (uint16_t)(~sum);
}

uint16_t alg_tcpudphdr_chksum(uint32_t src_addr, uint32_t dst_addr, uint8_t proto, const uint16_t *buff, uint16_t len)
{
    uint32_t sum = 0;

    sum += (src_addr & 0xffffUL);
    sum += ((src_addr >> 16) & 0xffffUL);
    sum += (dst_addr & 0xffffUL);
    sum += ((dst_addr >> 16) & 0xffffUL);
    sum += (uint32_t)htons((uint16_t)proto);
    sum += (uint32_t)htons(len);

    sum += alg_hdr_16bitsum(buff, len);

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return (uint16_t)(~sum);
}
