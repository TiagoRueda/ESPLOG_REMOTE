#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "wifi.c"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "lwip/sockets.h"
#include "addr_from_stdin.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/message_buffer.h"

#define BUFFER_SIZE_BYTES 2048
#define ITEM_SIZE 800
#define TAG "LOG"

MessageBufferHandle_t messageBufferHandle;
bool logToStdout = true;
char server_ip[20] = "192.168.1.15";
int server_port = 5000;

/**
 * @brief Estrutura para armazenar os parâmetros da tarefa TCP
 */
typedef struct {
	uint16_t connection_port;
	char ip_address[20];
	TaskHandle_t taskHandle;
} PARAMETER_t;

/**
 * @brief Função de logging customizada que redireciona logs para um buffer de mensagem
 */
int logging_vprintf(const char *fmt, va_list l) {
	char buffer[ITEM_SIZE];
	int buffer_len = vsprintf(buffer, fmt, l);
	if (buffer_len > 0) {
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		size_t sended = xMessageBufferSendFromISR(messageBufferHandle, &buffer, buffer_len, &xHigherPriorityTaskWoken);
		assert(sended == buffer_len);
	}
	if (logToStdout) {
		return vprintf( fmt, l );
	} else {
		return 0;
	}
}

/**
 * @brief Tarefa TCP que conecta a um servidor e envia dados recebidos de um buffer de mensagens
 */
void tcp_client(void *pvParameters)
{
	PARAMETER_t *task_parameter = pvParameters;
	PARAMETER_t param;
	memcpy((char *)&param, task_parameter, sizeof(PARAMETER_t));
	ESP_LOGI("TCP", "Iniciando tarefa TCP: porta=%d, IP=[%s]", param.connection_port, param.ip_address);

	int addr_family = 0;
	int ip_protocol = 0;
	bool fail_sock = false;

	struct sockaddr_in dest_addr;
	dest_addr.sin_addr.s_addr = inet_addr(param.ip_address);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(param.connection_port);
	addr_family = AF_INET;
	ip_protocol = IPPROTO_IP;

	ESP_LOGI("TCP", "Endereco: sin_addr=0x%" PRIx32, dest_addr.sin_addr.s_addr);
    if (dest_addr.sin_addr.s_addr == 0xffffffff) {
		struct hostent *hp;
		hp = gethostbyname(param.ip_address);
		if (hp == NULL) {
			ESP_LOGE("TCP", "Erro ao resolver hostname: %s", param.ip_address);
            vTaskDelete(param.taskHandle);
			return;
		}
		struct ip4_addr *ip4_addr;
		ip4_addr = (struct ip4_addr *)hp->h_addr;
		dest_addr.sin_addr.s_addr = ip4_addr->addr;
		ESP_LOGI("TCP", "Endereco: sin_addr=0x%" PRIx32, dest_addr.sin_addr.s_addr);
    }

	int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
	if (sock < 0) {
		fail_sock = true;
	}
	ESP_LOGI("TCP", "Socket criado, conectando a %s:%d", param.ip_address, param.connection_port);

	int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in6));
	if (err == 0) {
		ESP_LOGI("TCP", "Conexao estabelecida com sucesso");
	} else {
		ESP_LOGE("TCP", "Erro ao conectar socket: errno %d", errno);
        fail_sock = true;
	}

	xTaskNotifyGive(param.taskHandle);

	while (1) {
		char buffer[ITEM_SIZE];
		size_t received = xMessageBufferReceive(messageBufferHandle, buffer, sizeof(buffer), portMAX_DELAY);

		if (received > 0 && !fail_sock) {
			int ret = send(sock, buffer, received, 0);
			if(ret != received){
                ESP_LOGE("TCP", "Erro ao enviar dados: esperado=%d, enviado=%d", received, ret);
				shutdown(sock, 0);
				close(sock);
				vTaskDelete(NULL);
				break;
			}
		} else {
			ESP_LOGW("TCP", "Falha ao receber dados ou socket invalido");
            shutdown(sock, 0);
			close(sock);
			vTaskDelete(NULL);
			break;
		}
	}
}

void app_main(void) {
	wifi_main();
    ESP_LOGI(TAG, "Iniciando");
    messageBufferHandle = xMessageBufferCreate(BUFFER_SIZE_BYTES);
    configASSERT( messageBufferHandle );

    PARAMETER_t param;
    param.connection_port = server_port;
    strcpy(param.ip_address, server_ip);
    param.taskHandle = xTaskGetCurrentTaskHandle();
    xTaskCreate(tcp_client, "TCP", 5120, (void *)&param, 2, NULL);
    ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

    esp_log_set_vprintf(logging_vprintf);

    while(1){
        ESP_LOGI(TAG, "Aguardou...");
        vTaskDelay(5000);
    }
}
