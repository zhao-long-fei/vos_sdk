
#ifdef __cplusplus
extern "C"
{
#endif

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <time.h>
#include "utils.h"
#include "lstLib.h"
#include "cJSON.h"
#include "protocol_conversion.h"

#define SAFE_FREE(x)	{if (x)	free(x);	(x) = 0;}
#define DEV_ATTR_CONFIG_PATH	"/userdata/app/hy_gui/all_device_table.json"
#define SCENE_CONFIG_PATH		"/userdata/app/hy_gui/com_scenes_table.json"
cJSON *dev_array_js = NULL; 
cJSON *sence_array_js = NULL; 
pthread_mutex_t g_config_lock = PTHREAD_MUTEX_INITIALIZER;

/*设备类型表*/
dev_type_map_s hy_dev_type_map[] = {
	{"a1tNlnarBMz", DEV_TYPE_SWITCH},
	{"a1fC503XxQi", DEV_TYPE_SWITCH},
	{"a1843sgxZRL", DEV_TYPE_SWITCH},
	{"a1upECow3ho", DEV_TYPE_SWITCH},
	{"a18oUtSm3fd", DEV_TYPE_LIGHT},
	{"a1SbToUWOPE", DEV_TYPE_LIGHT},
	{"a16zWugdPlm", DEV_TYPE_CURTAIN},
	{"a1275xhvzLh", DEV_TYPE_CURTAIN},
	{"a1toh6SIRu1", DEV_TYPE_AIRCONDITIONER},
	{"a1nBa7b8nZT", DEV_TYPE_FRESHAIR},
	{"a1eFEb92Gjr", DEV_TYPE_FLOORHEAT},
	
	{"a1CKFOuptQq", DEV_TYPE_OUTLET},
	{"a1HPexKeWyj", DEV_TYPE_OUTLET},
	{"a1m5BjOb96k", DEV_TYPE_OUTLET},
	{"a1odDKk6Qc2", DEV_TYPE_OUTLET}
	
};

int get_dev_type_by_pk(char* pk)
{
	int type = DEV_TYPE_UNKNOW, size = 0, i = 0;

	if (NULL == pk) {
		printf("pk NULL\r\n");
		return type;
	}

	size = sizeof(hy_dev_type_map) / sizeof(hy_dev_type_map[0]);
	//printf("size is:%d\r\n", size);
	for (i = 0; i < size; i++) {
		if (0 == strcmp(hy_dev_type_map[i].product_key, pk)) {
			type = hy_dev_type_map[i].dev_type;
			break;
		}
	}	
	return type;
}

int json_config_open_by_name(char *path_name, cJSON **json_flie)
{
	int ret = 0;
    cJSON *json_root = NULL;
    char *buf = NULL;

    if ((NULL == path_name) || (NULL == json_flie)){
        printf("%s, input para err.\n", __FUNCTION__);
        return -1;
    }

	buf = file_read(path_name);
	if (NULL == buf) {
		printf("%s, file_read.\n", __FUNCTION__);
        return -2;
	}

    //printf("%s, path_name:%s json:\n%s\n", __FUNCTION__, path_name, buf);

    json_root = cJSON_Parse(buf);
    SAFE_FREE(buf);

    *json_flie = json_root;
    return ret;
}


int dev_search_config_read(char* file_path)
{
	int ret = 0;
	cJSON *json_array = NULL;
	
	if ((NULL == file_path)){
        printf("%s, input para err.\n", __FUNCTION__);
        return -1;
    }
	
	ret = json_config_open_by_name(file_path, &json_array);
	if (ret) {
		printf("%s, file open err ret:%d \n", __FUNCTION__, ret);
        return -1;
	}

EXIT:
	cJSON_Delete(json_array);
	return ret;
}


int refresh_search_config(int type)
{
	int ret = 0;
	
	pthread_mutex_lock(&g_config_lock);
	if (CONFIG_TYPE_DEVICE == type || CONFIG_TYPE_ALL == type) {
		printf("update device cfg\r\n");
		cJSON_Delete(dev_array_js);
		dev_array_js = NULL;
		json_config_open_by_name(DEV_ATTR_CONFIG_PATH, &dev_array_js);/*简化读取，直接保存json*/
	}

	if (CONFIG_TYPE_SCENE == type || CONFIG_TYPE_ALL == type) {
		printf("update sence cfg\r\n");
		cJSON_Delete(sence_array_js);
		sence_array_js = NULL;
		json_config_open_by_name(SCENE_CONFIG_PATH, &sence_array_js);/*简化读取，直接保存json*/
	}
	pthread_mutex_unlock(&g_config_lock);

	return ret;
}


#ifdef LINK_UI
int generate_ctrl_protocol_data(thing_search_info_s* search_info, thing_ctrl_protocol_s* ctrl_protocol)
{
	int ret = 0, item_num = 0, attr_num = 0, i = 0, j = 0, len = 0, find_dev_flag = 0, out_name_matched = 0;
	char decode[128] = {0}, tmp_data[256] = {0}, tmp_attr[256] = {0};
	char *dev_tmp = 
		"{\"ctrlType\":\"device\",\"PK\":\"%s\",\"serial\":\"%s\"}";
	char *attr_tmp = 
		"{\"value\":\"%s\",\"identifier\":\"%s\"}";
	char *sence_tmp = 
		"{\"ctrlType\":\"scene\",\"serial\":\"%s\"}";
	printf("##################################################\r\n");
	if (NULL == search_info || NULL == ctrl_protocol) {
		printf("%s, param err\r\n", __FUNCTION__);
		return -1;
	}

	cJSON* out_js = cJSON_CreateArray();
	cJSON* attr_out_js  = NULL, *tmp_js = NULL;

	pthread_mutex_lock(&g_config_lock);
	switch(search_info->search_type) {
	case DEV_NAME:
		if (dev_array_js) {
			item_num = cJSON_GetArraySize(dev_array_js);
			printf("%s, item_size:%d\r\n",__FUNCTION__, item_num);
		}
		for (i = 0; i < item_num; i++) {
			find_dev_flag = 0;
			out_name_matched = 0;
			cJSON* dev_js = cJSON_GetArrayItem(dev_array_js, i);
			cJSON* PK = cJSON_GetObjectItem(dev_js, "PK");
			cJSON* DN = cJSON_GetObjectItem(dev_js, "serial");
			cJSON* out_name = cJSON_GetObjectItem(dev_js, "name");
			cJSON* attribute_arr = cJSON_GetObjectItem(dev_js, "attribute");
			if (!PK || !DN) {
				continue;
			}
			/*按名字开关时，也检查类型，防止“打开开关”时，打开所有设备类型的开关*/
			char dev_type = get_dev_type_by_pk(PK->valuestring);
			if (DEV_TYPE_UNKNOW == dev_type) {
				printf("dev type unknow:%s\r\n", PK->valuestring);
				continue;
			}
			if ((DEV_TYPE_UNKNOW != search_info->dev_type) && (search_info->dev_type != dev_type)) {
				printf("dev_type:%d vs %d\r\n", search_info->dev_type, dev_type);
				continue;
			}
			/*******************************************************************/
			if (out_name && strstr(out_name->valuestring, search_info->name))
			{
				out_name_matched = 1;
				printf("this dev out name matched:%s\r\n", out_name->valuestring);
			}
			if (attribute_arr) {
				attr_num = cJSON_GetArraySize(attribute_arr);
				for (j = 0; j < attr_num; j++) {
					cJSON* attr_js = cJSON_GetArrayItem(attribute_arr, j);
					cJSON* attr_name = cJSON_GetObjectItem(attr_js, "name");
					cJSON* attr_id = cJSON_GetObjectItem(attr_js, "identifier");
					if (attr_name) {
						memset(decode, 0, sizeof(decode));
						mbedtls_base64_decode(decode, sizeof(decode) - 1, &len, attr_name->valuestring, strlen(attr_name->valuestring));
						printf("%s-----------------%s\r\n",attr_name->valuestring, decode);
						if ((strstr(decode, search_info->name) || out_name_matched) && strstr(attr_id->valuestring, search_info->identifier)) {
							if (0 == find_dev_flag) {
								find_dev_flag = 1;
								snprintf(tmp_data, 255, dev_tmp, PK->valuestring, DN->valuestring);
								tmp_js = cJSON_Parse(tmp_data);
								cJSON_AddItemToArray(out_js, tmp_js);
								attr_out_js = cJSON_CreateArray();
								cJSON_AddItemToObject(tmp_js, "attribute", attr_out_js);
							}
							snprintf(tmp_attr, 255, attr_tmp, search_info->value, attr_id->valuestring);
							cJSON_AddItemToArray(attr_out_js, cJSON_Parse(tmp_attr));
						}
					}
				} 
			}
		}
		
		break;
	case DEV_TYPE:
		if (dev_array_js) {
			item_num = cJSON_GetArraySize(dev_array_js);
			printf("%s, item_size:%d\r\n",__FUNCTION__, item_num);
		}
		for (i = 0; i < item_num; i++) {
			find_dev_flag = 0;
			cJSON* dev_js = cJSON_GetArrayItem(dev_array_js, i);
			cJSON* PK = cJSON_GetObjectItem(dev_js, "PK");
			cJSON* DN = cJSON_GetObjectItem(dev_js, "serial");
			cJSON* attribute_arr = cJSON_GetObjectItem(dev_js, "attribute");
			if (!PK || !DN) {
				continue;
			}
			char dev_type = get_dev_type_by_pk(PK->valuestring);
			if (DEV_TYPE_UNKNOW == dev_type) {
				printf("dev type unknow:%s\r\n", PK->valuestring);
				continue;
			}
			if (search_info->dev_type != dev_type) {
				continue;
			}
			if (attribute_arr) {
				attr_num = cJSON_GetArraySize(attribute_arr);
				for (j = 0; j < attr_num; j++) {
					cJSON* attr_js = cJSON_GetArrayItem(attribute_arr, j);
					cJSON* attr_name = cJSON_GetObjectItem(attr_js, "name");
					cJSON* attr_id = cJSON_GetObjectItem(attr_js, "identifier");
					if (attr_name) {
						memset(decode, 0, sizeof(decode));
						mbedtls_base64_decode(decode, sizeof(decode) - 1, &len, attr_name->valuestring, strlen(attr_name->valuestring));
						printf("%s-----------------%s\r\n",attr_name->valuestring, decode);
						if (strstr(attr_id->valuestring, search_info->identifier)) {
							if (0 == find_dev_flag) {
								find_dev_flag = 1;
								snprintf(tmp_data, 255, dev_tmp, PK->valuestring, DN->valuestring);
								tmp_js = cJSON_Parse(tmp_data);
								cJSON_AddItemToArray(out_js, tmp_js);
								attr_out_js = cJSON_CreateArray();
								cJSON_AddItemToObject(tmp_js, "attribute", attr_out_js);
							}
							snprintf(tmp_attr, 255, attr_tmp, search_info->value, attr_id->valuestring);
							cJSON_AddItemToArray(attr_out_js, cJSON_Parse(tmp_attr));
						}
					}
				} 
			}
		}
		break;
	case LOC_DEV_NAME:
		if (dev_array_js) {
			item_num = cJSON_GetArraySize(dev_array_js);
			printf("%s, item_size:%d\r\n",__FUNCTION__, item_num);
		}
		
		for (i = 0; i < item_num; i++) {
			find_dev_flag = 0;
			out_name_matched = 0;
			cJSON* dev_js = cJSON_GetArrayItem(dev_array_js, i);
			cJSON* PK = cJSON_GetObjectItem(dev_js, "PK");
			cJSON* DN = cJSON_GetObjectItem(dev_js, "serial");
			cJSON* space_name = cJSON_GetObjectItem(dev_js, "space_name");
			cJSON* out_name = cJSON_GetObjectItem(dev_js, "name");
			cJSON* attribute_arr = cJSON_GetObjectItem(dev_js, "attribute");
			if (!PK || !DN || !space_name) {
				continue;
			}
			char dev_type = get_dev_type_by_pk(PK->valuestring);
			if (DEV_TYPE_UNKNOW == dev_type) {
				printf("dev type unknow:%s\r\n", PK->valuestring);
				continue;
			}
			if ((DEV_TYPE_UNKNOW != search_info->dev_type) && (search_info->dev_type != dev_type)) {
				continue;
			}
			if (out_name && strstr(out_name->valuestring, search_info->name))
			{
				out_name_matched = 1;
				printf("this dev out name matched:%s\r\n", out_name->valuestring);
			}
			printf("location:%s vs %s\r\n", search_info->location, space_name->valuestring);
			if (strcmp(search_info->location, space_name->valuestring)) {
				continue;
			}
			if (attribute_arr) {
				attr_num = cJSON_GetArraySize(attribute_arr);
				for (j = 0; j < attr_num; j++) {
					cJSON* attr_js = cJSON_GetArrayItem(attribute_arr, j);
					cJSON* attr_name = cJSON_GetObjectItem(attr_js, "name");
					cJSON* attr_id = cJSON_GetObjectItem(attr_js, "identifier");
					if (attr_name) {
						memset(decode, 0, sizeof(decode));
						mbedtls_base64_decode(decode, sizeof(decode) - 1, &len, attr_name->valuestring, strlen(attr_name->valuestring));
						printf("%s-----------------%s\r\n",attr_name->valuestring, decode);
						if ((strstr(decode, search_info->name) || out_name_matched) && strstr(attr_id->valuestring, search_info->identifier)) {
							if (0 == find_dev_flag) {
								find_dev_flag = 1;
								snprintf(tmp_data, 255, dev_tmp, PK->valuestring, DN->valuestring);
								tmp_js = cJSON_Parse(tmp_data);
								cJSON_AddItemToArray(out_js, tmp_js);
								attr_out_js = cJSON_CreateArray();
								cJSON_AddItemToObject(tmp_js, "attribute", attr_out_js);
							}
							snprintf(tmp_attr, 255, attr_tmp, search_info->value, attr_id->valuestring);
							cJSON_AddItemToArray(attr_out_js, cJSON_Parse(tmp_attr));
						}
					}
				} 
			}
		}
		break;
	case LOC_DEV_TYPE:
		if (dev_array_js) {
			item_num = cJSON_GetArraySize(dev_array_js);
			printf("%s, item_size:%d\r\n",__FUNCTION__, item_num);
		}
		for (i = 0; i < item_num; i++) {
			find_dev_flag = 0;
			cJSON* dev_js = cJSON_GetArrayItem(dev_array_js, i);
			cJSON* PK = cJSON_GetObjectItem(dev_js, "PK");
			cJSON* DN = cJSON_GetObjectItem(dev_js, "serial");
			cJSON* space_name = cJSON_GetObjectItem(dev_js, "space_name");
			cJSON* attribute_arr = cJSON_GetObjectItem(dev_js, "attribute");
			char dev_type = get_dev_type_by_pk(PK->valuestring);
			if (DEV_TYPE_UNKNOW == dev_type) {
				printf("dev type unknow:%s\r\n", PK->valuestring);
				continue;
			}
			if (!PK || !DN || !space_name) {
				continue;
			}
			if (strcmp(search_info->location, space_name->valuestring)) {
				continue;
			}
			if (search_info->dev_type != dev_type) {
				continue;
			}
			if (attribute_arr) {
				attr_num = cJSON_GetArraySize(attribute_arr);
				for (j = 0; j < attr_num; j++) {
					cJSON* attr_js = cJSON_GetArrayItem(attribute_arr, j);
					cJSON* attr_name = cJSON_GetObjectItem(attr_js, "name");
					cJSON* attr_id = cJSON_GetObjectItem(attr_js, "identifier");
					if (attr_name) {
						memset(decode, 0, sizeof(decode));
						mbedtls_base64_decode(decode, sizeof(decode) - 1, &len, attr_name->valuestring, strlen(attr_name->valuestring));
						printf("%s-----------------%s\r\n",attr_name->valuestring, decode);
						if (strstr(attr_id->valuestring, search_info->identifier)) {
							if (0 == find_dev_flag) {
								find_dev_flag = 1;
								snprintf(tmp_data, 255, dev_tmp, PK->valuestring, DN->valuestring);
								tmp_js = cJSON_Parse(tmp_data);
								cJSON_AddItemToArray(out_js, tmp_js);
								attr_out_js = cJSON_CreateArray();
								cJSON_AddItemToObject(tmp_js, "attribute", attr_out_js);
							}
							snprintf(tmp_attr, 255, attr_tmp, search_info->value, attr_id->valuestring);
							cJSON_AddItemToArray(attr_out_js, cJSON_Parse(tmp_attr));
						}
					}
				} 
			}
		}	
		break;
	case SCENE_NAME:
		if (sence_array_js) {
			item_num = cJSON_GetArraySize(sence_array_js);
			printf("%s, sence_size:%d\r\n",__FUNCTION__, item_num);
		}
		for (i = 0; i < item_num; i++) {
			cJSON* sence_js = cJSON_GetArrayItem(sence_array_js, i);
			if (!sence_js) {
				return -1;
			}
			cJSON* sence_serial = cJSON_GetObjectItem(sence_js, "serial");
			cJSON* sence_name = cJSON_GetObjectItem(sence_js, "name");
			if (!sence_name || !sence_serial) {
				continue;
			}
			printf("%s----%s\r\n", sence_name->valuestring, search_info->name);
			
			if (strstr(sence_name->valuestring, search_info->name)) {
				printf("%s, item_size:%d\r\n",__FUNCTION__, item_num);
				snprintf(tmp_data, 255, sence_tmp, sence_serial->valuestring);
				cJSON_AddItemToArray(out_js, cJSON_Parse(tmp_data));
			}
			
		}
		break;
	case LOC_SCENE_NAME:

		break;
	default:
		break;
	}
	pthread_mutex_unlock(&g_config_lock);
	
	if (cJSON_GetArraySize(out_js) > 0) {
		ctrl_protocol->data = cJSON_PrintUnformatted(out_js);
		ctrl_protocol->length = strlen(ctrl_protocol->data);
		printf("ctrl data:%s\r\n", ctrl_protocol->data);
	}

	cJSON_Delete(out_js);
	return ret;
}
#endif

#ifdef __cplusplus
}
#endif


