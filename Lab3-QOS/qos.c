#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"


//srTCM
struct rte_meter_srtcmParams srtcmParams[APP_FLOWS_MAX];
struct rte_meter_srtcm srtcmData[APP_FLOWS_MAX]; 

//RED
struct rte_red_config redParams[e_RTE_METER_COLORS]; 
struct rte_red redData[APP_FLOWS_MAX][e_RTE_METER_COLORS]; 
uint32_t queueSize[APP_FLOWS_MAX][e_RTE_METER_COLORS] = {}; 
uint64_t pretime = 0; 

//clock
uint64_t cycle;
uint64_t hz;
/**
 * This function will be called only once at the beginning of the test. 
 * You can initialize your meter here.
 * 
 * int rte_meter_srtcm_config(struct rte_meter_srtcm *m, struct rte_meter_srtcmParams *params);
 * @return: 0 upon success, error code otherwise
 * 
 * void rte_exit(int exit_code, const char *format, ...)
 * #define rte_panic(...) rte_panic_(__func__, __VA_ARGS__, "dummy")
 * 
 * uint64_t rte_get_tsc_hz(void)
 * @return: The frequency of the RDTSC timer resolution
 * 
 * static inline uint64_t rte_get_tsc_cycles(void)
 * @return: The time base for this lcore.
 */
int
qos_meter_init(void)
{
    cycle = rte_get_tsc_cycles();
    hz = rte_get_tsc_hz();
    //1.28Gbps -> 1.28/8 = 160 Mbyte/s = 160,000,000 byte/s
    //burst size from 500 to 1500, packet size from 128 to 1152 byte
    srtcmParams[0].cir = 16000000; 
    srtcmParams[0].cbs = 640000; 
    srtcmParams[0].ebs = 1728000; 
    if (rte_meter_srtcm_config(srtcmData, srtcmParams)) 
        rte_panic("Cannot init srTCM data\n"); 
    srtcmParams[1].cir = 8000000; 
    srtcmParams[1].cbs = 320000; 
    srtcmParams[1].ebs = 864000; 
    if (rte_meter_srtcm_config(srtcmData+1, srtcmParams+1)) 
        rte_panic("Cannot init srTCM data\n"); 
    srtcmParams[2].cir = 4000000; 
    srtcmParams[2].cbs = 160000; 
    srtcmParams[2].ebs = 432000; 
    if (rte_meter_srtcm_config(srtcmData+2, srtcmParams+2)) 
        rte_panic("Cannot init srTCM data\n"); 
    srtcmParams[3].cir = 2000000; 
    srtcmParams[3].cbs = 80000; 
    srtcmParams[3].ebs = 216000; 
    if (rte_meter_srtcm_config(srtcmData+3, srtcmParams+3)) 
        rte_panic("Cannot init srTCM data\n"); 
    return 0;
}

/**
 * This function will be called for every packet in the test, 
 * after which the packet is marked by returning the corresponding color.
 * 
 * A packet is marked green if it doesn't exceed the CBS, 
 * yellow if it does exceed the CBS, but not the EBS, and red otherwise
 * 
 * The pkt_len is in bytes, the time is in nanoseconds.
 * 
 * Point: We need to convert ns to cpu circles
 * Point: Time is not counted from 0
 * 
 * static inline enum rte_meter_color rte_meter_srtcm_color_blind_check(struct rte_meter_srtcm *m,
	uint64_t time, uint32_t pkt_len)
 * 
 * enum qos_color { GREEN = 0, YELLOW, RED };
 * enum rte_meter_color { e_RTE_METER_GREEN = 0, e_RTE_METER_YELLOW,  
	e_RTE_METER_RED, e_RTE_METER_COLORS };
 */ 
enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    time = time * hz / 1000000000 + cycle;
    return rte_meter_srtcm_color_blind_check(&srtcmData[flow_id], time, pkt_len);
}


/**
 * This function will be called only once at the beginning of the test. 
 * You can initialize you dropper here
 * 
 * int rte_red_rt_data_init(struct rte_red *red);
 * @return Operation status, 0 success
 * 
 * int rte_red_config_init(struct rte_red_config *red_cfg, const uint16_t wq_log2, 
   const uint16_t min_th, const uint16_t max_th, const uint16_t maxp_inv);
 * @return Operation status, 0 success 
 */
int
qos_dropper_init(void)
{
    if (rte_red_config_init(&redParams[GREEN], RTE_RED_WQ_LOG2_MIN, 1022, 1023, 10)) 
        rte_panic("Cannot init GREEN config\n"); 
    if (rte_red_config_init(&redParams[YELLOW], RTE_RED_WQ_LOG2_MIN, 1022, 1023, 10)) 
        rte_panic("Cannot init YELLOW config\n"); 
    if (rte_red_config_init(&redParams[RED], RTE_RED_WQ_LOG2_MIN, 0, 1, 10)) 
        rte_panic("Cannot init RED config\n"); 
    int i; 
    for (i = 0; i < APP_FLOWS_MAX; ++i) {
        if (rte_red_rt_data_init(&redData[i][GREEN])) 
            rte_panic("Cannot init GREEN data\n"); 
        if (rte_red_rt_data_init(&redData[i][YELLOW])) 
            rte_panic("Cannot init YELLOW data\n");
        if (rte_red_rt_data_init(&redData[i][RED])) 
            rte_panic("Cannot init RED data\n"); 
    }
    return 0;
}

/**
 * This function will be called for every tested packet after being marked by the meter, 
 * and will make the decision whether to drop the packet by returning the decision (0 pass, 1 drop)
 * 
 * The probability of drop increases as the estimated average queue size grows
 * 
 * static inline void rte_red_mark_queue_empty(struct rte_red *red, const uint64_t time)
 * @brief Callback to records time that queue became empty
 * @param q_time : Start of the queue idle time (q_time) 
 * 
 * static inline int rte_red_enqueue(const struct rte_red_config *red_cfg,
	struct rte_red *red, const unsigned q, const uint64_t time)
 * @param q [in] updated queue size in packets   
 * @return Operation status
 * @retval 0 enqueue the packet
 * @retval 1 drop the packet based on max threshold criteria
 * @retval 2 drop the packet based on mark probability criteria
 */
int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    //fresh
    if (time != pretime) { 
        memset(queueSize, 0, sizeof(queueSize));
        int i, j;
        for (i = 0; i < APP_FLOWS_MAX; ++i)
            for (j = 0; j < e_RTE_METER_COLORS; ++j)
                rte_red_mark_queue_empty(&redData[i][j], time);
    }

    pretime = time;
    int result = rte_red_enqueue(&redParams[color], &redData[flow_id][color], queueSize[flow_id][color], time);
    if (!result)
        ++queueSize[flow_id][color];
    //pass
    return !!result;
}
