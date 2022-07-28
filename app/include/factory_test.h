#ifndef FCT_TEST
#define FCT_TEST

typedef void (*export_result_cb)(char *result);
void fct_audio_feed(char *buf, int len);
int fct_start(int channal);
void fct_init(export_result_cb export_result_cb);
void fct_deinit();

#endif