#ifndef __FANSTARGETAD_H_
#define __FANSTARGETAD_H_

#include "cand_user.h"
#include<vector>
#include<string>
#include <map>

typedef struct Imp_Info {
	string position;
	string impid;
} IMP_INFO;

typedef struct Access_Info: public AccessInfo {
	Access_Info():come_uid(0),impression_counts(0),appid(0),
	ip_code(0),debug(0),feed_position(""),ip("127.0.0.1"),exposure_id(""){}
	uint64_t come_uid;//当前来用户uid
	uint32_t impression_counts; //用户过去24小时看到了多少次广告
	uint32_t appid;
	uint32_t ip_code;
	//uint16_t gender;
	//uint16_t age;
	uint16_t num;   //当前需要出几个结果
	uint16_t debug;
	string feed_position; //当前feed广告位
	string ip;
	string exposure_id;//曝光id
	string group_id; //分组id
	string group_name;   //分组名

	string gender; 
	string age;
	string area_code;
	string city_code;
	string device_type;

	std::vector<IMP_INFO> imp_vec;
}ACCESS_INFO;


typedef struct Fans_Tar_Ad:public candidate_item_t {
	Fans_Tar_Ad(): type(0),adid(""),order_uid(0),bridge_uid(0),mid(""),buy_amount(0),
	max_package(0),finish_amount(0),begin_time(0),ad_type(0),brg_type(0),mark(""),mark_origin(""){}
	uint32_t type;//0表示用户对应粉条集合，1表示曝光不能完成订单 
	//uint64_t adid;//广告id
	string adid;//广告id
	string field_id; //非粉高级定向包，领域id
	string alg_log;	//算法层日志,分隔符;
	//uint32_t inflation; 扩包字段,没有默认为0 不需要了
	uint64_t order_uid;//广告主用户ID
	uint64_t bridge_uid;//该订单与该用户的桥梁信息，若没有，则为0
	string mid;//博文ID
	uint32_t buy_amount;//购买的数量
	uint32_t max_package;//最大包数量，若没有会赋值为0
	uint32_t finish_amount;//已经投放数量
	time_t begin_time; //订单起始时间
	uint32_t ad_type;//一个固定值，应用层传来，原样传回应用层
	uint32_t brg_type; // 一个固定值，应用层传来，原样传回
	uint32_t algBucket_id; //0 or 7:精准流量
	uint32_t source;//记录广告的来源
	string mark;
	string mark_origin;
	std::map<string, string> mark_map;
	std::map<string, string> mark_origin_map;
}FANS_TAR_AD;

typedef std::vector<FANS_TAR_AD*> FANS_VEC_CAND;

#endif