/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: dsmi declaration
 * Author:
 * Create: 2024-02-20
 */
 
#ifndef __DSMI_NETWORK_INTERFACE_API_H__
#define __DSMI_NETWORK_INTERFACE_API_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
 
#ifdef __linux
#define DLLEXPORT
#else
#define DLLEXPORT _declspec(dllexport)
#endif
// 带宽查询接口需要用到的数据结构
struct bandwidth_t {
    unsigned int time_interval;
    unsigned int tx_bandwidth;
    unsigned int rx_bandwidth;
};
// 测量带宽的默认时间与最大可用时间
#define DEFAULT_TIME_INTERVAL 1000
#define MIN_TIME_INTERVAL 100
#define MAX_TIME_INTERVAL 10000
 
// 网络流量包数统计数据结构
struct ds_port_stat_info {
    unsigned long long mac_tx_mac_pause_num;
    unsigned long long mac_rx_mac_pause_num;
    unsigned long long mac_tx_pfc_pkt_num;
    unsigned long long mac_tx_pfc_pri0_pkt_num;
    unsigned long long mac_tx_pfc_pri1_pkt_num;
    unsigned long long mac_tx_pfc_pri2_pkt_num;
    unsigned long long mac_tx_pfc_pri3_pkt_num;
    unsigned long long mac_tx_pfc_pri4_pkt_num;
    unsigned long long mac_tx_pfc_pri5_pkt_num;
    unsigned long long mac_tx_pfc_pri6_pkt_num;
    unsigned long long mac_tx_pfc_pri7_pkt_num;
    unsigned long long mac_rx_pfc_pkt_num;
    unsigned long long mac_rx_pfc_pri0_pkt_num;
    unsigned long long mac_rx_pfc_pri1_pkt_num;
    unsigned long long mac_rx_pfc_pri2_pkt_num;
    unsigned long long mac_rx_pfc_pri3_pkt_num;
    unsigned long long mac_rx_pfc_pri4_pkt_num;
    unsigned long long mac_rx_pfc_pri5_pkt_num;
    unsigned long long mac_rx_pfc_pri6_pkt_num;
    unsigned long long mac_rx_pfc_pri7_pkt_num;
    unsigned long long mac_tx_total_pkt_num;
    unsigned long long mac_tx_total_oct_num;
    unsigned long long mac_tx_bad_pkt_num;
    unsigned long long mac_tx_bad_oct_num;
    unsigned long long mac_rx_total_pkt_num;
    unsigned long long mac_rx_total_oct_num;
    unsigned long long mac_rx_bad_pkt_num;
    unsigned long long mac_rx_bad_oct_num;
    unsigned long long mac_rx_fcs_err_pkt_num;
    unsigned long long roce_rx_rc_pkt_num;
    unsigned long long roce_rx_all_pkt_num;
    unsigned long long roce_rx_err_pkt_num;
    unsigned long long roce_tx_rc_pkt_num;
    unsigned long long roce_tx_all_pkt_num;
    unsigned long long roce_tx_err_pkt_num;
    unsigned long long roce_cqe_num;
    unsigned long long roce_rx_cnp_pkt_num;
    unsigned long long roce_tx_cnp_pkt_num;
    unsigned long long roce_err_ack_num;
    unsigned long long roce_err_psn_num;
    unsigned long long roce_verification_err_num;
    unsigned long long roce_err_qp_status_num;
    unsigned long long roce_new_pkt_rty_num;
    unsigned long long roce_ecn_db_num;
    unsigned long long nic_tx_all_pkg_num;
    unsigned long long nic_tx_all_oct_num;
    unsigned long long nic_rx_all_pkg_num;
    unsigned long long nic_rx_all_oct_num;
    long tv_sec;
    long tv_usec;
    unsigned char reserved[64];
};

#define RESERVE_NUMBER_EXT                          6
struct ds_extra_statistics_info {
    unsigned long long cw_total_cnt;
    unsigned long long cw_before_correct_cnt;
    unsigned long long cw_correct_cnt;
    unsigned long long cw_uncorrect_cnt;
    unsigned long long cw_bad_cnt;
    unsigned long long trans_total_bit;
    unsigned long long cw_total_correct_bit;
    unsigned long long drop_num;
    unsigned long long pcs_err_count;
    unsigned long long rx_send_app_good_pkts;
    unsigned long long rx_send_app_bad_pkts;
    unsigned int reserved[RESERVE_NUMBER_EXT];
};

#define PING_MTU_NUM_MAX 3000
#define PING_PACKET_SIZE_MIN 1792
#define PING_PACKET_SIZE_MAX 3000
#define PING_PACKET_NUM_MIN 1
#define PING_PACKET_NUM_DEFAULT 3
#define PING_PACKET_NUM_MAX 1000
#define PING_PACKET_INTERVAL_MIN 0
#define PING_PACKET_INTERVAL_MAX 10
#define PING_PACKET_TIMEOUT_MIN 1
#define PING_PACKET_TIMEOUT_DEFAULT 10
#define PING_PACKET_TIMEOUT_MAX 20000
#define PING_SDID_INVALID_VALUE 0xFFFFFFFF
#define PING_IP_INVALID_VALUE 0
#define PING_DEVICE_ID_MAX 199
#define PING_CONSTRAINT_VALUE 20000

enum ping_mode {
    PING_ETH,
    PING_HCCS,
    PING_ROCE,
    PING_ROH,
    PING_MAX
};

enum ping_result {
    DS_PING_NOT_START,
    DS_PING_SUCCESS,
    DS_PING_SEND_FAILED,
    DS_PING_RECV_TIMEOUT,
    DS_PING_RES_MAX
};

// supernodes ping operate
typedef struct {
    char is_ipv6;               // 目前仅支持ipv4
    unsigned int sdid;
    unsigned int packet_size;
    unsigned int packet_send_num;
    unsigned int packet_interval;
    unsigned int timeout;
    unsigned int get_info;
    int phy_id;
    unsigned char reserved[24];
} hccs_ping_operate_info;

// supernodes ping reply
enum hccs_ping_result {
    RESULT_PING_NOT_START,
    RESULT_PING_SUCCESS,       // 报文拷贝回、且校验ok
    RESULT_PING_SEND_FAILED,   // 抢占通道失败，即报文发送失败
    RESULT_PING_CHECK_FAILED,  // 报文拷贝回来，校验失败
};

typedef struct {
    unsigned char ret[PING_PACKET_NUM_MAX];
    unsigned char ret_16k[PING_PACKET_NUM_MAX];
    unsigned int total_packet_send_num;
    unsigned int total_packet_recv_num;
    unsigned int dstip;         // 接收在device侧sdid值转换的ip地址
    long time[PING_PACKET_NUM_MAX];
} hccs_ping_reply_info;

// traceroute info
#define __TRACEROUTE_INFO
#define DEVDRV_IPV6_ARRAY_INT_LEN                   4
struct dsmi_traceroute_info {
    int max_ttl;
    int tos;
    int waittime;
    int source_port;
    int dest_port;
    unsigned int dip[DEVDRV_IPV6_ARRAY_INT_LEN];
    bool ipv6_flag;
};

// pfc_durtation_info
#define PRI_NUM  8
struct pfc_duration_info {
    unsigned long long tx[PRI_NUM];
    unsigned long long rx[PRI_NUM];
};
#define MAX_TC_STAT_LEN  8
struct ds_tc_stat_data {
    unsigned long long tc_tx[MAX_TC_STAT_LEN];
    unsigned long long tc_rx[MAX_TC_STAT_LEN];
    unsigned long long reserved[MAX_TC_STAT_LEN];
};

#define MAX_QP_NUM                        8192
struct ds_qpn_list {
    unsigned int qp_num;
    unsigned int qpn_list[MAX_QP_NUM];
};
 
 
#define IP_ADDRESS_LEN   40
struct ds_qp_info {
    unsigned char status;        // qp状态
    unsigned char type;          // qp类型
    char ip[IP_ADDRESS_LEN];     // 目标ip地址
    unsigned short src_port;     // 源端口
    unsigned int src_qpn;        // 源qp号
    unsigned int dst_qpn;        // 目标qp号
    unsigned int send_psn;       // 发送的psn
    unsigned int recv_psn;       // 接收的psn
    char reserved[32];           // 保留字段
};

struct ds_cdr_mode_info {
    unsigned char mode;
    unsigned char master_flag;             /* 主从die标识 */
};

#define HCCS_PING_MESH_MAX_NUM 48

struct hccs_ping_mesh_operate {
    unsigned int sdid[HCCS_PING_MESH_MAX_NUM];
    int dest_num; // 有多少个待ping的目的
    int phy_id; // 源的phy id
    int ping_task_period; // 任务轮询间隔
    int ping_pkt_size; // 报文大小
    int ping_pkt_num; //  报文发送数量
    int ping_pkt_interval; // 报文发送间隔
    int task_id; // 任务号
    unsigned char reserved[32];
};

#define ADDR_MAX_LEN            16
struct ping_mesh_stat {
    char dest_addr[HCCS_PING_MESH_MAX_NUM][ADDR_MAX_LEN];
    unsigned int ping_success_num[HCCS_PING_MESH_MAX_NUM]; // 成功数量
    unsigned int ping_fail_num[HCCS_PING_MESH_MAX_NUM]; // 失败数量
    long ping_max_time[HCCS_PING_MESH_MAX_NUM]; // ping最大时间
    long ping_min_time[HCCS_PING_MESH_MAX_NUM]; // ping最小时间
    long ping_avg_time[HCCS_PING_MESH_MAX_NUM]; // ping平均时间
    long ping_95[HCCS_PING_MESH_MAX_NUM];       // ping 95分位数时延
    int reply_stat_num[HCCS_PING_MESH_MAX_NUM]; // 当前综合值结合了多少轮的ping结果
    unsigned long long ping_total_num[HCCS_PING_MESH_MAX_NUM]; // 一共ping了多少轮
    int dest_num;
    unsigned char L1_plane_check_res[HCCS_PING_MESH_MAX_NUM];
    unsigned char reserved[HCCS_PING_MESH_MAX_NUM];
};
/**
* @ingroup network
* @brief  Get the rdma bandwidth
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port The port of the device
* @param [out] bandwidth_info The detailed bandwidth info
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_get_bandwidth(int logic_id, int port, struct bandwidth_t *bandwidth_info);
 
/**
* @ingroup network
* @brief  Get the pkt num info
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port The port of the device
* @param [out] ds_port_stat_info The detailed pkt num info
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_get_netdev_stat(int logic_id, int port, struct ds_port_stat_info *stat);

/**
* @ingroup network
* @brief  Get the ping pkt info
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port The port of the device
* @param [in] ping_info The ping operation parameters
* @param [out] reply_info The information returned by the ping operation
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_hccs_ping(int logic_id, int port_id, hccs_ping_operate_info *ping_info,
                             hccs_ping_reply_info *reply_info);

/**
* @brief  Get the traceroute process running status
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [out] troute_status The running status
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_get_traceroute_status(int logic_id, int *troute_status);

/**
* @ingroup network
* @brief  Run traceroute cmd
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] traceroute_info_send The traceroute param to send
* @param [out] troute_start_result The sesult of running the cmd
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_start_traceroute(int logic_id, struct dsmi_traceroute_info *traceroute_info_send,
    int *troute_start_result);

/**
* @ingroup network
* @brief  Get the traceroute response info
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [out] troute_info_show The response info
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_get_traceroute_info(int logic_id, char *troute_info_show, unsigned int info_size, int cmd_type);

/**
* @ingroup network
* @brief  Kill the traceroute process
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [out] troute_reset The result of kill cmd
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_reset_traceroute(int logic_id, int *troute_reset);

/**
* @ingroup network
* @brief  set prbs flag
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] prbs_flag prbs_flag
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_set_prbs_flag(int logic_id, int prbs_flag);

/**
* @ingroup network
* @brief  set prbs tx/rx adapt in order
* @attention NULL
* @param [in] mode 0:prbs tx serdes tx trigger, 1:prbs tx retimer host adapt,
            2:prbs rx serdes media adapt, 3:prbs rx retimer media adapt
* @param [in] logic_id  The logic device id
* @param [in] master_flag  The device is master or slave
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_prbs_adapt_in_order(unsigned int mode, unsigned int logic_id, unsigned char master_flag);

/**
* @ingroup network
* @brief  get device tc info
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] stat
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_get_tc_stat(int logic_id, struct ds_tc_stat_data *stat);

/**
* @ingroup network
* @brief  set device tc info
* @attention NULL
* @param [in] logic_id  The logic device id
* @return  0 for success, others for fail
*/
DLLEXPORT int dsmi_clear_tc_stat(int logic_id);

/**
* @ingroup network
* @brief  set cdr mode
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port
* @param [in] info mode: 0: stop eth adapt config\n"
*                        1: start eth adapt config\n"
*                  master_flag: 0: master die\n"
*                               1: slave die\n"
* @return  0 for success, others for fail
*/

int dsmi_set_cdr_mode_cmd(int logic_id, int port, struct ds_cdr_mode_info *info);

/**
* @ingroup network
* @brief  set cdr mode
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] info pfc_duration_time
* @return  0 for success, others for fail
*/

int dsmi_get_pfc_duration_info(int logic_id, struct pfc_duration_info *info);

/**
* @ingroup network
* @brief  clear pfc duration
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] mode 0: clear all\n"
* @return  0 for success, others for fail
*/
int dsmi_clear_pfc_duration(int logic_id, int mode);

/**
* @ingroup network
* @brief  get qpn list
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port_id   Specify the network port number, reseverd field
* @param [out] list The response qpn list
* @return  0 for success, others for fail
*/
int dsmi_get_qpn_list(int logic_id, int port_id, struct ds_qpn_list *list);
 
/**
* @ingroup network
* @brief  get qpn list
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port_id   Specify the network port number, reseverd field
* @param [in] qpn  The query qp number
* @param [out] qp_info The response qp info
* @return  0 for success, others for fail
*/
int dsmi_get_qp_info(int logic_id, int port_id, unsigned int qpn, struct ds_qp_info *qp_info);

/**
* @ingroup network
* @brief  start ping mesh task
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] operate_info operation info
* @return  0 for success, others for fail
*/
int dsmi_start_hccs_ping_mesh(int logic_id, struct hccs_ping_mesh_operate *operate_info);

/**
* @ingroup network
* @brief  get ping mesh result info
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] task_id  The ping mesh task id: 0 or 1
* @param [out] info ping mesh result info
* @return  0 for success, others for fail
*/
int dsmi_get_hccs_ping_mesh_info(int logic_id, int task_id, struct ping_mesh_stat *info);

/**
* @ingroup network
* @brief  get ping mesh task state
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] task_id  The ping mesh task id: 0 or 1
* @param [out] state ping mesh task state: 0 stop; 1 running
* @return  0 for success, others for fail
*/
int dsmi_get_hccs_ping_mesh_state(int logic_id, int task_id, unsigned int *state);

/**
* @ingroup network
* @brief  stop ping mesh task
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] task_id  The ping mesh task id: 0 or 1
* @return  0 for success, others for fail
*/
int dsmi_stop_hccs_ping_mesh(int logic_id, int task_id);

/**
* @ingroup network
* @brief  get extra statistics info
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] port_id  The port id: 0;
* @param [out] info extra statistics info;
* @return  0 for success, others for fail
*/
int dsmi_get_extra_statistics_info(int logic_id, int port_id, struct ds_extra_statistics_info *info);

/**
* @ingroup network
* @brief  set offline nic down flag
* @attention NULL
* @param [in] logic_id  The logic device id
* @param [in] enable_flag  enable flag : 0:disable, 1:enable
* @return  0 for success, others for fail
*/
int dsmi_set_device_offline_nic_down_flag(int logic_id, int enable_flag);
#ifdef __cplusplus
}
#endif
#endif
