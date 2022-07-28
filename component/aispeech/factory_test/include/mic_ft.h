#ifndef __MIC_TEST_H__
#define __MIC_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SYMBOL_IMPORT_OR_EXPORT __attribute ((visibility("default")))

typedef struct ft_engine ft_engine_t;

typedef struct ft_test {
	ft_engine_t *ft_enging;
	int          ft_start_flag;
} ft_test_t;

/**
 * 创建实例
 *
 * @param  conf [配置文件路径]
 * @return 实例指针，NULL表示失败
 */
SYMBOL_IMPORT_OR_EXPORT ft_engine_t *ft_engine_new(int channelCount, int sineBin, int fs);

/**
 * 送入音频数据，进行处理
 * @param  engine [实例指针]
 * @param  data   [音频数据]
 * @param  len    [音频长度]
 * @return -1：送入的音频不足够计算
 *          0：送入的音频足够用于计算
 */
SYMBOL_IMPORT_OR_EXPORT int ft_engine_feed(ft_engine_t *engine, signed char *data, int len);

/**
 * 计算结果
 * @param  engine      [实例指针]
 * @param  result_json [计算结果，以json格式输出]
 * @return  -1：送入的音频不够，计算失败
 *           0：计算成功
 */
SYMBOL_IMPORT_OR_EXPORT int ft_engine_calc(ft_engine_t *engine, char **result_json);

/**
 * 销毁实例
 * @param engine [实例指针]
 */
SYMBOL_IMPORT_OR_EXPORT void ft_engine_delete(ft_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
