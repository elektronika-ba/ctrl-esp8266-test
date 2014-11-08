#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "driver/uart.h"

#include "ctrl_config_server.h"
#include "flash_param.h"

static struct espconn esp_conn;
static esp_tcp esptcp;

static void ICACHE_FLASH_ATTR ctrl_config_server_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	char tmp[100];

    os_sprintf(tmp, "webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);

    uart0_sendStr(tmp);
}

static void ICACHE_FLASH_ATTR ctrl_config_server_discon(void *arg)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	char tmp[100];

    os_sprintf(tmp, "webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);

    uart0_sendStr(tmp);
}

static void ICACHE_FLASH_ATTR ctrl_config_server_recv(void *arg, char *data, unsigned short len)
{
	struct espconn *ptrespconn = (struct espconn *)arg;

	uart0_sendStr("RECV: ");
	unsigned short i;
	for(i=0; i<len; i++)
	{
		char tmp2[5];
		os_sprintf(tmp2, "%c", data[i]);
		uart0_sendStr(tmp2);
	}
	uart0_sendStr("\r\n");

/*
    URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;
    parse_flag = save_data(pusrdata, length);

    do {
        if (parse_flag == false) {
        	response_send(ptrespconn, false);
        	if (dat_sumlength == 0){
        		if (precvbuffer != NULL){
        			os_free(precvbuffer);
        			precvbuffer = NULL;
        		}
        	}
            break;
        }

//        os_printf(precvbuffer);
        pURL_Frame = (URL_Frame *)os_zalloc(sizeof(URL_Frame));
        parse_url(precvbuffer, pURL_Frame);

        switch (pURL_Frame->Type) {
            case GET:
                os_printf("We have a GET request.\n");

                if (os_strcmp(pURL_Frame->pSelect, "client") == 0 &&
                        os_strcmp(pURL_Frame->pCommand, "command") == 0) {
                    if (os_strcmp(pURL_Frame->pFilename, "info") == 0) {
                        json_send(ptrespconn, INFOMATION);
                    }

                    if (os_strcmp(pURL_Frame->pFilename, "status") == 0) {
                        json_send(ptrespconn, CONNECT_STATUS);
                    } else if (os_strcmp(pURL_Frame->pFilename, "scan") == 0) {
                        char *strstr = NULL;
                        strstr = (char *)os_strstr(pusrdata, "&");

                        if (strstr == NULL) {
                            if (pscaninfo == NULL) {
                                pscaninfo = (scaninfo *)os_zalloc(sizeof(scaninfo));
                            }

                            pscaninfo->pespconn = ptrespconn;
                            pscaninfo->pagenum = 0;
                            pscaninfo->page_sn = 0;
                            pscaninfo->data_cnt = 0;
                            wifi_station_scan(NULL, json_scan_cb);
                        } else {
                            strstr ++;

                            if (os_strncmp(strstr, "page", 4) == 0) {
                                if (pscaninfo != NULL) {
                                    pscaninfo->pagenum = *(strstr + 5);
                                    pscaninfo->pagenum -= 0x30;

                                    if (pscaninfo->pagenum > pscaninfo->totalpage || pscaninfo->pagenum == 0) {
                                        response_send(ptrespconn, false);
                                    } else {
                                        json_send(ptrespconn, SCAN);
                                    }
                                } else {
                                    response_send(ptrespconn, false);
                                }
                            } else {
                                response_send(ptrespconn, false);
                            }
                        }
                    } else {
                        response_send(ptrespconn, false);
                    }
                } else if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
                           os_strcmp(pURL_Frame->pCommand, "command") == 0) {
                    if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0) {
                        ap_conf = (struct softap_config *)os_zalloc(sizeof(struct softap_config));
                        sta_conf = (struct station_config *)os_zalloc(sizeof(struct station_config));
                        json_send(ptrespconn, WIFI);
                        os_free(sta_conf);
                        os_free(ap_conf);
                        sta_conf = NULL;
                        ap_conf = NULL;
                    }

#if PLUG_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0) {
                        json_send(ptrespconn, SWITCH_STATUS);
                    }

#endif

#if LIGHT_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "light") == 0) {
                        json_send(ptrespconn, LIGHT_STATUS);
                    }

#endif

                    else if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0) {
                        json_send(ptrespconn, REBOOT);
                    } else {
                        response_send(ptrespconn, false);
                    }
                } else {
                    response_send(ptrespconn, false);
                }

                break;

            case POST:
                os_printf("We have a POST request.\n");
                pParseBuffer = (char *)os_strstr(precvbuffer, "\r\n\r\n");

                if (pParseBuffer == NULL) {
                    break;
                }

                pParseBuffer += 4;

                if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
                        os_strcmp(pURL_Frame->pCommand, "command") == 0) {
#if SENSOR_DEVICE

                    if (os_strcmp(pURL_Frame->pFilename, "sleep") == 0) {
#else

                    if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0) {
#endif

                        if (pParseBuffer != NULL) {
                            if (restart_10ms != NULL) {
                                os_timer_disarm(restart_10ms);
                            }

                            if (rstparm == NULL) {
                                rstparm = (rst_parm *)os_zalloc(sizeof(rst_parm));
                            }

                            rstparm->pespconn = ptrespconn;
#if SENSOR_DEVICE
                            rstparm->parmtype = DEEP_SLEEP;
#else
                            rstparm->parmtype = REBOOT;
#endif

                            if (restart_10ms == NULL) {
                                restart_10ms = (os_timer_t *)os_malloc(sizeof(os_timer_t));
                            }

                            os_timer_setfn(restart_10ms, (os_timer_func_t *)restart_10ms_cb, NULL);
                            os_timer_arm(restart_10ms, 10, 0);  // delay 10ms, then do

                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    } else if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0) {
                        if (pParseBuffer != NULL) {
                            struct jsontree_context js;
                            user_esp_platform_set_connect_status(DEVICE_CONNECTING);

                            if (restart_10ms != NULL) {
                                os_timer_disarm(restart_10ms);
                            }

                            if (ap_conf == NULL) {
                                ap_conf = (struct softap_config *)os_zalloc(sizeof(struct softap_config));
                            }

                            if (sta_conf == NULL) {
                                sta_conf = (struct station_config *)os_zalloc(sizeof(struct station_config));
                            }

                            jsontree_setup(&js, (struct jsontree_value *)&wifi_req_tree, json_putchar);
                            json_parse(&js, pParseBuffer);

                            if (rstparm == NULL) {
                                rstparm = (rst_parm *)os_zalloc(sizeof(rst_parm));
                            }

                            rstparm->pespconn = ptrespconn;
                            rstparm->parmtype = WIFI;

                            if (sta_conf->ssid[0] != 0x00 || ap_conf->ssid[0] != 0x00) {
                                ap_conf->ssid_hidden = 0;
                                ap_conf->max_connection = 4;

                                if (restart_10ms == NULL) {
                                    restart_10ms = (os_timer_t *)os_malloc(sizeof(os_timer_t));
                                }

                                os_timer_disarm(restart_10ms);
                                os_timer_setfn(restart_10ms, (os_timer_func_t *)restart_10ms_cb, NULL);
                                os_timer_arm(restart_10ms, 10, 0);  // delay 10ms, then do
                            } else {
                                os_free(ap_conf);
                                os_free(sta_conf);
                                os_free(rstparm);
                                sta_conf = NULL;
                                ap_conf = NULL;
                                rstparm =NULL;
                            }

                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    }

#if PLUG_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0) {
                        if (pParseBuffer != NULL) {
                            struct jsontree_context js;
                            jsontree_setup(&js, (struct jsontree_value *)&StatusTree, json_putchar);
                            json_parse(&js, pParseBuffer);
                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    }

#endif

#if LIGHT_DEVICE
                    else if (os_strcmp(pURL_Frame->pFilename, "light") == 0) {
                        if (pParseBuffer != NULL) {
                            struct jsontree_context js;

                            jsontree_setup(&js, (struct jsontree_value *)&PwmTree, json_putchar);
                            json_parse(&js, pParseBuffer);
                            response_send(ptrespconn, true);
                        } else {
                            response_send(ptrespconn, false);
                        }
                    }

#endif
                    else {
                        response_send(ptrespconn, false);
                    }
                } else {
                    response_send(ptrespconn, false);
                }

//	            os_free(pParseBuffer);
                break;
        }

        if (precvbuffer != NULL){
        	os_free(precvbuffer);
        	precvbuffer = NULL;
        }
        os_free(pURL_Frame);
        pURL_Frame = NULL;

    } while (0);
*/
}

static void ICACHE_FLASH_ATTR ctrl_config_server_listen(void *arg)
{
    struct espconn *pesp_conn = (struct espconn *)arg;

	uart0_sendStr("ctrl_config_server_listen\r\n");

    espconn_regist_recvcb(pesp_conn, ctrl_config_server_recv);
    espconn_regist_reconcb(pesp_conn, ctrl_config_server_recon);
    espconn_regist_disconcb(pesp_conn, ctrl_config_server_discon);
}

// all socket data which is received is flushed into this function
void ICACHE_FLASH_ATTR ctrl_config_server_init()
{
    esptcp.local_port = 80;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    espconn_regist_connectcb(&esp_conn, ctrl_config_server_listen);

    espconn_accept(&esp_conn);
}
