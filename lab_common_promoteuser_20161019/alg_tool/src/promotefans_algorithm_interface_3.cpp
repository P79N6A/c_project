#include "promotefans_algorithm_interface_3.h"

DYN_ALGORITHMS(PromoteFansAlgorithmInterface);

int PromoteFansAlgorithmInterface::algorithm_core(uint64_t req_id, const AccessStr& access_str, VEC_CAND& vec_cand){

	LOG_ERROR("PromoteFansAlgorithmInterface::algorithm_core return 1\n");
	return 1;
}

bool dict_compare(pair<int, float> lhs, pair<int, float> rhs){
        return lhs.second > rhs.second;
}
bool User_Order_Ad_compare(User_Order_Ad * lhs, User_Order_Ad * rhs){
        return lhs->adscore > rhs->adscore;
}
int PromoteFansAlgorithmInterface::algorithm_core_new(uint64_t req_id, const AccessInfo* access_info, const VEC_CAND& input_vec, VEC_CAND& output_vec){
	ACCESS_INFO* ai = (ACCESS_INFO*) access_info;

	int size = input_vec.size();
	LOG_ERROR("PromoteFansAlgorithmInterface inputvec.size():%d\n", size);
	if(size <= 0){
		LOG_ERROR("no input ads, req_id:%u", req_id);
		return 1;
	}

//	user_ad_history_ctr(ai, input_vec, output_vec, ai->ad_num);
	try {
		user_ad_ctr_estimate(ai, input_vec, output_vec, ai->ad_num);
	} catch (const char* msg) {
		cerr << msg << endl;
		LOG_ERROR("",msg);

	}

	return 1;
}
/**
 *
 * @param ai 用户个人信息
 * @param input_vec 广告候选集
 * @param output_vec 返回数据
 * @param num 总共需要返回的广告数
 * @return
 */
int PromoteFansAlgorithmInterface::user_ad_history_ctr(const ACCESS_INFO* ai, const VEC_CAND& input_vec,VEC_CAND& output_vec, int num){
	//1. 取lushan数据
	LOG_ERROR("In history_ctr");
	DbInterface* p_insuff_order_interface = p_db_company_->get_db_interface("NONFANS_USER_PROFILE");
	if(NULL == p_insuff_order_interface){
		LOG_ERROR("lushan connect error!");
		return -1;
	}
	uint16_t key_size = input_vec.size() + 1;
	char** keystr = new char*[key_size];
	for(uint16_t i = 0; i < key_size; i++)
		keystr[i] = new char[256];
	int index = 0;
	float defaultctr = 0.0;
	//2. get ader

	for(VEC_CAND::const_iterator it = input_vec.begin();
			it!=input_vec.end(); it++){
		User_Order_Ad * ft = (User_Order_Ad*)(*it);
		sprintf(keystr[index++],"%u-%"PRIu64"", 3, ft->uid);//order_id info;
	}

	sprintf(keystr[index],"%u-1", 3);//默认ctr
	map<uint64_t, const char*> result;
	int count = 0;
	map<int, float> sort_ad;
	string token = ",";
	vector<string> user_profile;
	float score = 0.0;
	string def;
	int redis_flag = ((McDbInterface *)p_insuff_order_interface)->mget(1, keystr, key_size, result);
	if(redis_flag != 1){
		LOG_ERROR("redis_flag_error");
		//随机出广告
		for(VEC_CAND::const_iterator it = input_vec.begin();it!=input_vec.end(); it++){
			score = getRandomFloat();
			User_Order_Ad * ft = (User_Order_Ad*)(*it);
			ft->adscore = floor(score * 100000);
			sort_ad.insert(make_pair(count, score));
			LOG_ERROR("AD:%d %s ADER SCORE RANDOM:%lf", count,ft->order_id.c_str(), score);
			count++;	
		}
		goto FREE_MODULE;
	}
	LOG_ERROR("EXPECT:%u GET: %u", key_size, result.size());
	for(map<uint64_t, const char*>::const_iterator iter = result.begin();iter != result.end(); iter++){
		LOG_ERROR("LUSHAN %lld:%s", iter->first, iter->second);
	}

	// 直接比拼ader的  ctr
	if (result.find(1) != result.end()){
		def = result[1];
		defaultctr = atof(def.c_str());
	}
	else defaultctr = 0.00043;

	for(VEC_CAND::const_iterator it = input_vec.begin();
			it!=input_vec.end(); it++){
		User_Order_Ad * ft = (User_Order_Ad*)(*it);
		if(result.find(ft->uid)!=result.end()){
			string str = result[ft->uid];
			score = atof(str.c_str());
		}else
			score = defaultctr;

		ft->adscore = floor(score * 100000);
		sort_ad.insert(make_pair(count, score));
		LOG_ERROR("AD:%d %s ADER SCORE:%lf", count,ft->order_id.c_str(), score);
		count++;
	}

	//释放内存 
FREE_MODULE:    
	for(uint16_t i=0; i < key_size; i++)
		delete[] keystr[i];
	if(keystr!=NULL)
		delete[] keystr;

	if (sort_ad.size() >= 1){
		vector<pair<int, float> > tmp(sort_ad.begin(), sort_ad.end());
		sort(tmp.begin(), tmp.end(), dict_compare);
		if (sort_ad.size()<3) num = sort_ad.size();
		for (int i = 0;i<num;i++){
			output_vec.push_back(input_vec[tmp[i].first]);
		}
		LOG_ERROR("BEST:%lf", tmp[0].second);
	}       
	return 1;
}


void PromoteFansAlgorithmInterface::_RunThread(){
	vector<pair<int,int> > model_vec;
	vector<pair<int,int> >::iterator iter;
	//first is lushan db number; second is model conf
	model_vec.push_back(make_pair(MODEL_LR_DB_NO,23));
	if(model_vec.size() >0 ){
		model_conf* mcf;
		mcf = new model_conf[model_vec.size()];
		// 初始化配置参数
		for(iter = model_vec.begin(); iter < model_vec.end(); iter++){
			mcf[iter - model_vec.begin()].update_f = "-1";
		}
		while(1){
			//{db_key:[21-12,21-13];mapkey:[13:0];arraykey:12;indexkey:13;update:11}
			//{db_key:[21-21,21-22];mapkey:[22:0];arraykey:21;indexkey:22;update:11}
			for(iter = model_vec.begin(); iter < model_vec.end(); iter++){
				int flg = parse_model_parameters(mcf[iter - model_vec.begin()],iter->first,iter->second);
				if(flg == 1){
					LOG_ERROR("_RunThread() update model dbnum:%d,conf_key:%d Update",iter->first,iter->second);
					this->update_model(mcf[iter - model_vec.begin()],&model_copy_ptr,&model_ptr[iter - model_vec.begin()],iter->first,iter->second);
				}
				else{
					LOG_ERROR("_RunThread() update model dbnum:%d,conf_key:%d  Not Need Update ",iter->first,iter->second);
				}

			}
			sleep(60 * 60);//设置间隔时间 测试设置1小时
		}
		delete[] mcf;
		if (mcf != NULL) mcf = NULL;
	}
}
int PromoteFansAlgorithmInterface::parse_model_parameters(model_conf &mcf,int db_num,int key){
	//get lushan  model配置
	DbInterface* p_insuff_order_interface = p_db_company_->get_db_interface("NONFANS_USER_PROFILE");
	//申请变量
	vector<string> tmp_vec;
	string token =";",info="";
	int idx = 0,debug =1,update_flg = 0;
	int redis_flag = 0,model_keysize = 1;
	map<uint64_t,const char*> lsresult;
	string conf_str;
	//参数初始化
	mcf.keys_vec.clear();
	mcf.mapkeys.clear();
	mcf.arraykey = -1;
	char** lskeys = new char*[1];
	try{
		lskeys[0] = new char[128]();
	}catch (const char* msg) {
		cerr << msg << endl;
		goto FREE_MODEL;
	}
	sprintf(lskeys[0],"%d-%d",db_num,key);
	//obtain lushan data
	if(NULL == p_insuff_order_interface){
		LOG_ERROR("lushan connect error!");
	}
	else redis_flag = ((McDbInterface *)p_insuff_order_interface)->mget(1, lskeys, model_keysize, lsresult);
	if (redis_flag !=1){
		LOG_ERROR("lushan obtain data error!");
		goto FREE_MODEL;
	}
	if (lsresult.find(key) != lsresult.end() && lsresult[key] != NULL && strlen(lsresult[key])>2){
		conf_str = lsresult[key];
	}
	else {
		LOG_ERROR("lushan obtained data check is error!");
		goto FREE_MODEL;
	}
	if (debug ==1) LOG_ERROR("lushan model obtained data: db-key:%d-%d,val:%s",db_num,key,conf_str.c_str());
	info = conf_str.substr(1,conf_str.length()-2);
	SplitString(info,token,tmp_vec);
	for(size_t i=0;i< tmp_vec.size();i++){
		idx = tmp_vec[i].find(":");
		LOG_ERROR("lushan model obtain 1 parameters %s,%s",tmp_vec[i].substr(0,idx).c_str(),tmp_vec[i].substr(idx+1).c_str());
		//check 分割后 k-v 都不为空
		if ( idx == -1 || idx ==0 || idx+2>tmp_vec[i].length()){
			continue;
		}
		LOG_ERROR("lushan model obtain 2 parameters %s,%s",tmp_vec[i].substr(0,idx).c_str(),tmp_vec[i].substr(idx+1).c_str());
		if (strcmp(tmp_vec[i].substr(0,idx).c_str(),"db_key") == 0){
			token = ",";
			info = tmp_vec[i].substr(idx+2,tmp_vec[i].length()-idx-3);
			SplitString(info,token,mcf.keys_vec);


		}
		else if (strcmp(tmp_vec[i].substr(0,idx).c_str(),"mapkey") == 0){
			token = ",";
			info = tmp_vec[i].substr(idx+2,tmp_vec[i].length()-idx-3);
			SplitString(info,token,mcf.mapkeys);
		}
		else if (strcmp(tmp_vec[i].substr(0,idx).c_str(),"arraykey") == 0){
			//该字段可以不用，检查是否为空
			mcf.arraykey = atoi(tmp_vec[i].substr(idx+1,tmp_vec[i].length()-idx-1).c_str());
		}
		else if (strcmp(tmp_vec[i].substr(0,idx).c_str(),"indexkey") == 0){
			//该字段可以不用，检查是否为空
			mcf.indexkey = atoi(tmp_vec[i].substr(idx+1,tmp_vec[i].length()-idx-1).c_str());
		}
		else if (strcmp(tmp_vec[i].substr(0,idx).c_str(),"update") == 0){
			string flg = tmp_vec[i].substr(idx+1,tmp_vec[i].length()-idx-1);
			if ( strcmp(mcf.update_f.c_str(),flg.c_str()) !=0) {
			//	mcf.update_f = flg;
				update_flg = 1;
			}
			else{
				goto FREE_MODEL;
			}
		}
		//新增字段 自己调用使用
		else {
			LOG_ERROR("lushan model obtain 3 parameters %s,%s",tmp_vec[i].substr(0,idx).c_str(),tmp_vec[i].substr(idx+1).c_str());
			mcf.parameters.insert(make_pair(tmp_vec[i].substr(0,idx),atoi(tmp_vec[i].substr(idx+1).c_str())));
		}

	}
	//字段检查
	if(mcf.keys_vec.size()<1 || mcf.mapkeys.size()<1){
		update_flg = 0;
		LOG_ERROR("lushan model obtained error data: lushan keys num:%d mapkeys num :%d",mcf.keys_vec.size(),mcf.mapkeys.size());
	}
FREE_MODEL:
	if (lskeys != NULL) delete lskeys[0];
	if (lskeys != NULL ) delete lskeys;
	LOG_ERROR("lushan model obtain finish");
	return update_flg;

}
/**
 * 模型更新函数
 * @param mcf 配置文件
 * @param copy_ptr 备份的模型
 * @param ptr 最新的模型
 * @param lsdb_num
 * @param conf_key 配置文件在lushan库中的key
 */
void PromoteFansAlgorithmInterface::update_model(model_conf &mcf,model_data** copy_ptr, model_data** ptr,int lsdb_num,int conf_key){
	time_t now;
	time(&now);
	LOG_ERROR("MODEL UPDATE BEGIN PTHREAD:%d,dbnum:%d,conf_key:%d",tid_fans_economy,lsdb_num,conf_key);
	//申请模型内存  申请新模型空间struct
	model_data* m_ptr =  new model_data();
	if(mcf.parameters.size() != 0) m_ptr->mapNum = mcf.mapkeys.size()+1;
	else m_ptr->mapNum = mcf.mapkeys.size();
	LOG_ERROR("mapNum:%d",m_ptr->mapNum);
	char** m_weights = NULL;
	m_ptr->weightsNum = 0;
	Map** map_i = new Map*[m_ptr->mapNum]();
	for( uint16_t i=0;i<m_ptr->mapNum;i++){
		map_i[i] = new Map();
	}
	//准备获取lushan数据进行解析
	map<uint64_t, const char*> result;
	vector<string> tmp_vector;
	string token = ";";
	uint16_t model_keysize = 2;
	if (mcf.keys_vec.size() != 0) model_keysize = mcf.keys_vec.size();
	char** keystr = new char*[model_keysize];
	for (uint16_t i = 0;i<model_keysize;++i){
		keystr[i] = new char[256]();
		sprintf(keystr[i],"%s",mcf.keys_vec[i].c_str());//model key
	}
	//设置模型更新数据量的标记，以检查模型是否更新正确
	int feature_newdata_num = 0 ,weight_newdata_num = 0,debug = 1;
	//connect lushan
	DbInterface* p_insuff_order_interface = p_db_company_->get_db_interface("NONFANS_USER_PROFILE");
	int redis_flag = 0;
	if(NULL == p_insuff_order_interface){
		LOG_ERROR("lushan connect error!");
	}
	else redis_flag = ((McDbInterface *)p_insuff_order_interface)->mget(1, keystr, model_keysize, result);
	if(debug == 1) LOG_ERROR("MODEL UPDATE LUSHAN DATA EXPECT :%u GET: %u", model_keysize, result.size());
	//解析数组型模型数据
	if( mcf.arraykey !=-1 && result.find(mcf.arraykey) != result.end()){
		tmp_vector.clear();
		string info = result[mcf.arraykey];
		SplitString(info, token,tmp_vector);
		//find max array number
		int max_num = -1;
		for(size_t i = 0;i<tmp_vector.size();i++){
			int idx = tmp_vector[i].find(':');
			int key = atoi(tmp_vector[i].substr(0,idx).c_str());
			if (key > max_num) max_num = key;
		}
		m_ptr->weightsNum = max_num+1;
		m_weights = new char*[m_ptr->weightsNum];
		for (int i = 0; i < m_ptr->weightsNum; i++){
			m_weights[i] = new char[256]();
		}
		//if(debug == 1) LOG_ERROR("MODEL UPDATE ARRAY DATA LUSHAN key:%d   value: %s",mcf.arraykey, info.c_str());
		if(debug == 1) LOG_ERROR("MODEL UPDATE ARRAY DATA LUSHAN key:%d item num:%d ",mcf.arraykey,tmp_vector.size());
		for(uint32_t i=0;i<tmp_vector.size();++i){
			int idx = tmp_vector[i].find(':');
			int key = atoi(tmp_vector[i].substr(0,idx).c_str());
			//考虑异常安全性，对写入key范围进行检查
			if( key < 0 || key >= m_ptr->weightsNum )
				LOG_ERROR("MODEL UPDATE ARRAR DATA ITERM ERROR key:%d value:%f", key,atof(tmp_vector[i].substr(idx+1).c_str()));
			else{
				sprintf(m_weights[key],"%f",atof(tmp_vector[i].substr(idx+1).c_str()));
				weight_newdata_num++;
			}
			//if(debug == 1) LOG_ERROR("MODEL UPDATE ARRAR DATA ITERM CHECK key:%d value:%f", key,atof(tmp_vector[i].substr(idx+1).c_str()));
		}
	}
	//lushan数据错误，跳出
	if(redis_flag != 1){
		LOG_ERROR("redis_flag_error");
		goto FREE_MODULE;
	}
	//解析MAP型模型数据 特征离散数据
	for(int mk=0;mk < m_ptr->mapNum;mk++){
		if (debug ==1) LOG_ERROR("MODEL UPDATE MAP DATA Iter:%d,%d,%d,%d",mk,mcf.parameters.size(),mcf.mapkeys.size(),m_ptr->mapNum);
		if ( mcf.parameters.size() != 0 && mk == m_ptr->mapNum-1 ){
			Map::iterator iter;
			for(iter = mcf.parameters.begin(); iter != mcf.parameters.end();iter++){
				if (debug ==1) LOG_ERROR("MODEL UPDATE MAP DATA  parameters %s,%d",iter->first.c_str(),iter->second);
				(map_i[mk])->insert(make_pair(iter->first,iter->second));
			}
			break;
		}
		tmp_vector.clear();
		int idx = mcf.mapkeys[mk].find(":");
		int qkey = atoi(mcf.mapkeys[mk].substr(0,idx).c_str());
		int m_pos = atoi(mcf.mapkeys[mk].substr(idx+1).c_str());
		LOG_ERROR("MAP %d,%d %s",qkey,m_pos,mcf.mapkeys[mk].c_str());
		if(result.find(qkey) != result.end()){
			string info = result[qkey];
			SplitString(info, token,tmp_vector);
			if (debug ==1) LOG_ERROR("MODEL UPDATE MAP LUSHAN key:%d GET: %s", qkey,info.c_str());
		}
		if (debug ==1) LOG_ERROR("MODEL UPDATE MAP DATA key:%d item num:%d ",qkey, tmp_vector.size());
		for(uint32_t i=0;i<tmp_vector.size();++i){
			idx = tmp_vector[i].find(':');
			if (idx != -1 && idx >2 && idx+1 <= tmp_vector[i].size()){
				//if (debug ==1) LOG_ERROR("MODEL UPDATE MAP DATA ITERM CHECK: key:%s, value:%s",tmp_vector[i].substr(0,idx).c_str(),tmp_vector[i].substr(idx+1).c_str());
				if(m_pos < m_ptr->mapNum && m_pos >=0){
					(map_i[m_pos])->insert(make_pair(tmp_vector[i].substr(0,idx).c_str(),atoi(tmp_vector[i].substr(idx+1).c_str())));
					feature_newdata_num++;
				}else{
					LOG_ERROR("input error m_pos :>= mapnum");
				}
			}
		}
		if(mcf.indexkey == qkey) m_ptr->indexkey = m_pos;
		if (debug ==1) LOG_ERROR("MODEL UPDATE MAP DATA key:%d map_pos:%d insert num:%d ",qkey,m_pos,feature_newdata_num);
	}
	//检查模型新数据量是正确,错误则goto free model
	if(feature_newdata_num < 100 || weight_newdata_num < 100 ){
		LOG_ERROR("MODEL UPDATE ERRO FREE MODEL PTHREAD:%d,dbnum:%d,conf_key:%d",tid_fans_economy,lsdb_num,conf_key);
		redis_flag =0;
		goto FREE_MODULE;
	}
	m_ptr->mapArray = map_i;
	m_ptr->weights = m_weights;
	//加锁 更新模型
	pthread_mutex_lock(&p_lock_fans_economy);
	LOG_ERROR("addlock update:%d",tid_fans_economy);
	*copy_ptr = *ptr;
	*ptr = m_ptr;
	//解锁
	pthread_mutex_unlock(&p_lock_fans_economy);
	LOG_ERROR("releaselock update:%d",tid_fans_economy);
	sleep(3);
	//释放原模型内存
	if((*copy_ptr)->weights != NULL){
		for(int i=0;i<(*copy_ptr)->weightsNum;++i){
			if ((*copy_ptr)->weights[i] != NULL)
				delete[] (*copy_ptr)->weights[i];
		}
		if ((*copy_ptr)->weights != NULL)
			delete (*copy_ptr)->weights;
	}
	if((*copy_ptr)->mapArray != NULL){
		for(uint16_t i=0;i<(*copy_ptr)->mapNum;i++){
			if ((*copy_ptr)->mapArray[i] != NULL)
			delete (*copy_ptr)->mapArray[i];
		}
		if ((*copy_ptr)->mapArray != NULL)
			delete (*copy_ptr)->mapArray;
	}
	//new 出来的要delete掉
	delete *copy_ptr;
	if(debug == 1) LOG_ERROR("MODEL UPDATE FINISHED UPDATE PTHREAD:%d,dbnum:%d,conf_key:%d",tid_fans_economy,lsdb_num,conf_key);
	// free memery
FREE_MODULE:
	//释放临时空间
	for (uint32_t i = 0; i<model_keysize; ++i){
		if (keystr[i] != NULL)
		delete[] keystr[i];
	}
	if (keystr != NULL) delete keystr;
	//模型内存未使用，释放
	if(redis_flag != 1){
		if(debug == 1) LOG_ERROR("MODEL UPDATE ERRO FREE MODEL PTHREAD:%d,dbnum:%d,conf_key:%d",tid_fans_economy,lsdb_num,conf_key);
		if(m_weights != NULL){
			for(int i=0;i<m_ptr->weightsNum;++i){
				if (m_weights[i]!=NULL)
				delete[] m_weights[i];
			}
			if(m_weights != NULL) delete m_weights;
		}
		if(map_i != NULL){
			for(int i=0;i<m_ptr->mapNum;++i){
				if (map_i[i] != NULL)
				delete map_i[i];
			}
			if(map_i != NULL) delete map_i;
		}
		delete m_ptr;
	}
	if(debug == 1) LOG_ERROR("MODEL UPDATE TASK FINISHED  PTHREAD:%d,dbnum:%d,conf_key:%d",tid_fans_economy,lsdb_num,conf_key);
}

/**
 *ctr预估
 * @param ai 用户数据
 * @param input_vec 广告候选集
 * @param output_vec 返回排好序的广告
 * @param res_ad_num 需要返回的数量
 * @return
 */
int PromoteFansAlgorithmInterface::user_ad_ctr_estimate(const ACCESS_INFO* ai, const VEC_CAND& input_vec,VEC_CAND& output_vec, int res_ad_num){
	LOG_ERROR("user_ad_ctr_estimate begin...uid: %u, adnum: %d", ai->come_uid,res_ad_num);
	if (input_vec.size() == 0) {
		LOG_ERROR("no input");
		return 1;
	}
	if (input_vec.size() < res_ad_num) {
		res_ad_num = input_vec.size();
	}
	//用户数据
	int gender = ai->gender;
	int os = ai->os;
	//广告主各项ctr数据
	map<uint64_t, Ad_Info> ad_infos = get_ad_infos(input_vec);
	if(ad_infos.size()==0){
		LOG_ERROR("lushan connect error!");
		return 1;
	}
	LOG_ERROR("get ad info done");
	Ad_Info default_ad_info = get_default_ad_info();
	LOG_ERROR("get default ad info done");
	//模型数据
	model_read_ptr = read_model();

	//特征映射，拼接key，查找对应的映射
	//查找权重
	//计算分值
	vector<User_Order_Ad*> tmp_new;
	for(VEC_CAND::const_iterator it = input_vec.begin(); it!=input_vec.end(); it++){
		User_Order_Ad * ft = (User_Order_Ad*)(*it);
		if(ad_infos.find(ft->uid)==ad_infos.end()){
			continue;
		}
		Ad_Info ai = ad_infos[ft->uid];
		float adScore = getAdScore(ai, model_read_ptr, default_ad_info);
		float genderScore = getGenderScore(gender, ai, model_read_ptr, default_ad_info);
		float platformScore = getPlatformScore(os, ai, model_read_ptr, default_ad_info);

		float score =  adScore + genderScore + platformScore;
		ft->adscore=floor(score * 100000);
		LOG_ERROR("adid: %u, adScore: %u",ai.adid, ft->adscore);
		tmp_new.push_back(ft);
	}
	sort(tmp_new.begin(), tmp_new.end(), User_Order_Ad_compare);//排序

	//结果放到output_vec中
	for (int i = 0; i < res_ad_num; i++) {
		User_Order_Ad * ft = tmp_new[i];
		LOG_ERROR("final adid: %u,  adScore: %u", ft->uid, ft->adscore);//结果展示一下
		output_vec.push_back(ft);
	}

	return 1;
}


/**
 *获取所有请求的广告的各项ctr数据
 如果数据库连接失败，则返回NULL
 * @param input_vec
 * @return
 */
map<uint64_t, Ad_Info> PromoteFansAlgorithmInterface::get_ad_infos(const VEC_CAND& input_vec){
	map<uint64_t, Ad_Info> res_ad_infos;
	int dbno = ADER_PROFILE_DB_NO;
	//取lushan数据
	LOG_ERROR("get ad info");
	DbInterface* p_insuff_order_interface = p_db_company_->get_db_interface("NONFANS_USER_PROFILE");
	if(NULL == p_insuff_order_interface){
		return res_ad_infos;
	}
	string COMMA = ",";
	uint16_t key_size = input_vec.size() + 1;
	char** keystr = new char*[key_size];
	for(uint16_t i = 0; i < key_size; i++)
		keystr[i] = new char[256];
	int index = 0;
	//拼接广告id
	for(VEC_CAND::const_iterator it = input_vec.begin(); it!=input_vec.end(); it++){
		User_Order_Ad * ft = (User_Order_Ad*)(*it);
		sprintf(keystr[index++],"%u-%"PRIu64"", dbno, ft->uid);//数据库库号， 广告主id
	}

	map<uint64_t, const char*> result;
	LOG_ERROR("pre pare");
	int mc_flag = ((McDbInterface *)p_insuff_order_interface)->mget(1, keystr, key_size, result);
	LOG_ERROR("db done");

	std::map<uint64_t, const char*>::iterator iter;
	 for( iter = result.begin(); iter!=result.end(); iter++){
		 LOG_ERROR("ADID: %u, ctr: %s", iter->first, iter->second);
	 }
	if(mc_flag != 1){
		LOG_ERROR("mc_flag_error");
		goto FREE_MODULE;
	}
	//拼接广告数据到res_ad_infos中
	for(VEC_CAND::const_iterator it = input_vec.begin(); it!=input_vec.end(); it++){
		User_Order_Ad * ft = (User_Order_Ad*)(*it);
		Ad_Info ai;
		string s;
		if(result.find(ft->uid)!=result.end()){
			s = result[ft->uid];
		}else if(result.find(1)!=result.end()){//默认值
			s = result[1];
		}else{//如果数据库里默认值也没有
			s = "0.00073,0.000678,0.000585,0.000416,0.000907,0.000339";
		}
		vector<string> fileds;
		SplitString(s, COMMA, fileds);
		if(fileds.size()<6){//如果数据库有的数据长度太小，有问题
			s = "0.00073,0.000678,0.000585,0.000416,0.000907,0.000339";
			SplitString(s, COMMA, fileds);
		}
		string ctr = fileds[0];
		string male_ctr = fileds[1];
		string female_ctr =fileds[2];
		string ios_ctr =fileds[3];
		string android_ctr = fileds[4];
		string other_ctr = fileds[5];
		ai.adid = ft->uid;
		ai.ctr = ctr;
		ai.male_ctr = male_ctr;
		ai.female_ctr = female_ctr;
		ai.ios_ctr = ios_ctr;
		ai.android_ctr = android_ctr;
		ai.other_ctr = other_ctr;

		res_ad_infos.insert(make_pair(ft->uid, ai));
	}
FREE_MODULE:
	//释放内存
	for (uint16_t i = 0; i < key_size; i++) {
		delete[] keystr[i];
	}
	if (keystr != NULL) {
		delete[] keystr;
	}
	return res_ad_infos;
}

/**
 * 读模型
 * @return
 */
model_data* PromoteFansAlgorithmInterface::read_model(){
	//ctr模型
	pthread_mutex_lock(&p_lock_fans_economy);
	LOG_ERROR("addlock search:%d",tid_fans_economy);
	model_read_ptr = model_ptr[0];
	LOG_ERROR("releaselock search:%d",tid_fans_economy);
	//LOG_ERROR("LOAD MODEL BEGIN LOCK1 SLEEP PTHREAD:%d",tid_fans_economy);
	pthread_mutex_unlock(&p_lock_fans_economy);
	return model_read_ptr;
}

int PromoteFansAlgorithmInterface::search_feature_index(string& key, model_data* read_ptr){
	int index = -1,debug = 0;
	if(read_ptr->mapArray[read_ptr->indexkey]->find(key.c_str()) != read_ptr->mapArray[read_ptr->indexkey]->end()){
		index = (*(read_ptr->mapArray[read_ptr->indexkey]))[key.c_str()];
		if (1 == debug) LOG_ERROR("MODEL UPDATE CHECK MAP COMPARE :key_s-%s,value:%d",key.c_str(),(*(read_ptr->mapArray[read_ptr->indexkey]))[key.c_str()]);
	}
	return index;
}

/**
 *搜索权重
 * @param feature_name
 * @param value
 * @param read_ptr
 * @param value_type 0是浮点数，非0是整数。
 * @return
 */
float PromoteFansAlgorithmInterface::search_weights(string feature_name, string value,model_data *read_ptr,int value_type){
	//value_type 0：float，1：int
	LOG_ERROR("MODEL UPDATE CHECK: search_weights, feature name: %s, value: %s",feature_name.c_str(), value.c_str());
	if (read_ptr == NULL || read_ptr->weights == NULL ){
		LOG_ERROR("MODEL UPDATE CHECK: read_ptr is NULL");
		return 0.0;
	}
	int val_length = value.length();
	float query_value = atof(value.c_str());
	char val[128];
	if (value_type) {
		sprintf(val,"%d",int(query_value));
	}
	else{
		sprintf(val,"%s",value.c_str());
	}
	string key = feature_name + "_"+ val;
	int k = search_feature_index(key,read_ptr);
	int back_i = 1;
	while(-1 == k && back_i < 4 ){
		query_value = query_value *(1- back_i *back_i / 50.0);
		if (value_type ==1 ) {
			sprintf(val,"%d",int(query_value));
		}
		else{
			sprintf(val,"%s",value.c_str());
			if(val_length - back_i >0)
			val[val_length - back_i] = 0;
		}
		key = feature_name+"_" + val;

		k = search_feature_index(key,read_ptr);
		++back_i;
	}
	if(-1 != k) {
		LOG_ERROR("MODEL UPDATE CHECK:weights %s: %f",key.c_str(),atof(read_ptr->weights[k]));
		return atof(read_ptr->weights[k]);
	}else{
		LOG_ERROR("k== -1: %s, value: %s",key.c_str());
	}
	return -10000;
}
/**
 * 获取广告主平均ctr
 * @param ai
 * @param model_read_ptr
 * @return
 */
float PromoteFansAlgorithmInterface::getAdScore(Ad_Info ai, model_data *model_read_ptr, Ad_Info default_ad_info){
	float res_score;
	string feature_name;
	feature_name="ctr";
	res_score = search_weights(feature_name,ai.ctr,model_read_ptr,VALUE_TYPE);
	if (int(res_score) == FAIL_WEIGHT) {
		res_score = atof(default_ad_info.ctr.c_str());
	}
	return res_score;
}
/**
 *获取性别交叉评分
 * @param gender
 * @param ai
 * @param model_read_ptr
 * @return
 */
float PromoteFansAlgorithmInterface::getGenderScore(int gender, Ad_Info ai, model_data *model_read_ptr, Ad_Info default_ad_info){
	float res_score;
	string feature_name;
	if(gender==0){
		feature_name="male";
		res_score = search_weights(feature_name,ai.male_ctr,model_read_ptr,VALUE_TYPE);
		if (int(res_score) == FAIL_WEIGHT) {
			res_score = atof(default_ad_info.male_ctr.c_str());
		}
	}else{
		feature_name="female";
		res_score = search_weights(feature_name,ai.female_ctr,model_read_ptr,VALUE_TYPE);

		if (int(res_score) == FAIL_WEIGHT) {
			res_score = atof(default_ad_info.female_ctr.c_str());
		}
	}

	return res_score;
}
/**
 * 获取平台交叉评分
 todo 确定平台的id号
 * @param platform
 * @param ai
 * @param model_read_ptr
 * @return
 */
float PromoteFansAlgorithmInterface::getPlatformScore(int platform, Ad_Info ai, model_data *model_read_ptr, Ad_Info default_ad_info){
	float res_score;
	string feature_name;
	if(platform==1){
		feature_name="ios";
		res_score = search_weights(feature_name,ai.ios_ctr,model_read_ptr,VALUE_TYPE);
		if (int(res_score) == FAIL_WEIGHT) {
			res_score = atof(default_ad_info.ios_ctr.c_str());
		}
	}else if(platform==2){
		feature_name="android";
		res_score = search_weights(feature_name,ai.android_ctr,model_read_ptr,VALUE_TYPE);
		if (int(res_score) == FAIL_WEIGHT) {
			res_score = atof(default_ad_info.android_ctr.c_str());
		}
	}else{
		feature_name="other";
		res_score = search_weights(feature_name,ai.other_ctr,model_read_ptr,VALUE_TYPE);
		if (int(res_score) == FAIL_WEIGHT) {
			res_score = atof(default_ad_info.other_ctr.c_str());
		}
	}
	return res_score;
}
Ad_Info PromoteFansAlgorithmInterface::get_default_ad_info(){
	Ad_Info res;
	string s ;
	vector<string> fileds;
	string ctr;
	string male_ctr;
	string female_ctr;
	string ios_ctr;
	string android_ctr;
	string other_ctr;
	LOG_ERROR("get default ad info");
	DbInterface* p_insuff_order_interface = p_db_company_->get_db_interface("NONFANS_USER_PROFILE");
	if(NULL == p_insuff_order_interface){
		return res;
	}
	string COMMA = ",";
	uint16_t key_size = 1;
	char** keystr = new char*[key_size];
	for(uint16_t i = 0; i < key_size; i++){
		keystr[i] = new char[256];
	}
	int index = 0;

	sprintf(keystr[index],"%u-1", ADER_PROFILE_DB_NO);//默认ctr
	map<uint64_t, const char*> result;
	int redis_flag = ((McDbInterface *)p_insuff_order_interface)->mget(1, keystr, key_size, result);
	if(redis_flag != 1){
		LOG_ERROR("redis_flag_error");
		goto FREE_MODULE;
	}
	if(result.find(1)!=result.end()){
		s = result[1];
	}else{
		s = "0.00073,0.000678,0.000585,0.000416,0.000907,0.000339";
	}
	//拼接广告数据到res_ad_infos中
	SplitString(s, COMMA, fileds);
	if(fileds.size()<6){//如果数据库有的数据长度太小，有问题
		s = "0.00073,0.000678,0.000585,0.000416,0.000907,0.000339";
		SplitString(s, COMMA, fileds);
	}
	ctr = fileds[0];
	male_ctr = fileds[1];
	female_ctr =fileds[2];
	ios_ctr =fileds[3];
	android_ctr = fileds[4];
	other_ctr = fileds[5];
	res.adid = 1;
	res.ctr = ctr;
	res.male_ctr = male_ctr;
	res.female_ctr = female_ctr;
	res.ios_ctr = ios_ctr;
	res.android_ctr = android_ctr;
	res.other_ctr = other_ctr;

	FREE_MODULE:
		//释放内存
		for (uint16_t i = 0; i < key_size; i++) {
			delete[] keystr[i];
		}
		if (keystr != NULL) {
			delete[] keystr;
		}
		return res;
	return res;
}
