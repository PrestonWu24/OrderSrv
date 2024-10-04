
#ifndef _POSITION_PROTOCOL_H_
#define _POSITION_PROTOCOL_H_


// position scope: 3001 - 4000


// ---- message type ----
#define msg_posi_position           3001
#define msg_posi_init_finish        3002
#define msg_posi_stop_trade         3003
#define msg_posi_identify           3004
#define msg_posi_init_product       3005
// send sum position and pandl value
#define msg_posi_cal_value          3006
#define msg_posi_exit               3007
#define msg_posi_checkpoint         3008
#define msg_posi_finish_modify      3009
#define msg_posi_start_trade        3010
#define msg_posi_heartbeat          3011
#define msg_posi_heartbeat_request  3012
#define msg_posi_calendar_exit      3013
#define msg_posi_fill_spread        3014
#define msg_posi_fill_outright      3015

// ---- field ----
#define fid_posi_symbol             3001
#define fid_posi_xtt_qty            3002
#define fid_posi_fix_qty            3003
#define fid_posi_client_name        3004
#define fid_posi_source             3005
#define fid_posi_manual_qty         3006
#define fid_posi_sum_qty            3007
#define fid_posi_price              3008
#define fid_posi_settle_price       3009
// CL, HO, RB
#define fid_posi_pro_type           3010
#define fid_posi_pandl              3011
#define fid_posi_session            3012
#define fid_posi_seq_num            3013
#define fid_posi_account            3014
#define fid_posi_tt_modify          3015
#define fid_posi_heartbeat_interval 3016

// ---- error code ----


#endif
