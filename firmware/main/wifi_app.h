// wifi_app.h — 连 WiFi + 用 TCP 把帧数据发出去（PC 端不用插串口也能看）
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifi_app_start(void);
bool wifi_app_connected(void);

/* 广播一行文本给已连接的客户端；没人连就直接丢掉，不阻塞 */
void wifi_app_broadcast(const char *line, int len);

#ifdef __cplusplus
}
#endif
