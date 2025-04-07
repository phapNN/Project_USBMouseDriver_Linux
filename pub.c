#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"

// --- Cấu hình thông tin MQTT ---
#define ADDRESS     "ssl://05e0816ba798455ab55c337172ebb78e.s1.eu.hivemq.cloud:8883"
#define CLIENTID    "Phap"
#define TOPIC       "PHAP/MOUSE_DATA"
#define USERNAME    "nhatphap_mqtt"
#define PASSWORD    "Phap123456"
#define QOS         0
#define TIMEOUT     10000L
#define DATA_FILE   "/home/nhatphap/Desktop/mouse_data_full.txt"

int main() {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    // Tạo client
    printf("🔧 Đang tạo MQTT client...\n");
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // Cấu hình SSL
    ssl_opts.enableServerCertAuth = 1;

    // Cấu hình kết nối
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.ssl = &ssl_opts;

    // Kết nối đến MQTT broker
    printf("🔌 Đang kết nối đến MQTT broker...\n");
    int rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "❌ Không thể kết nối tới MQTT broker: %s\n", MQTTClient_strerror(rc));
        return EXIT_FAILURE;
    }
    printf("✅ Kết nối thành công!\n");

    // Mở file dữ liệu
    FILE *fp;
    char line[512];

    while (1) {
        fp = fopen(DATA_FILE, "r");
        if (!fp) {
            perror("❌ Không thể mở file dữ liệu");
            MQTTClient_disconnect(client, 10000);
            MQTTClient_destroy(&client);
            return EXIT_FAILURE;
        }

        // Đọc dòng đầu tiên trong file và gửi qua MQTT
        if (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0'; // Xoá newline

            pubmsg.payload = line;
            pubmsg.payloadlen = strlen(line);
            pubmsg.qos = QOS;
            pubmsg.retained = 0;

            rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
            if (rc != MQTTCLIENT_SUCCESS) {
                fprintf(stderr, "❌ Gửi dữ liệu lỗi: %s\n", MQTTClient_strerror(rc));
            } else {
                printf("📤 Đã gửi: %s\n", line);
                MQTTClient_waitForCompletion(client, token, TIMEOUT);
            }
        } else {
            printf("⚠️ Không có dữ liệu trong file.\n");
        }

        fclose(fp);

        // Đợi 1 giây trước khi gửi lại dòng đầu tiên
        sleep(1);
    }

    // Dọn dẹp
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    printf("✅ Hoàn tất gửi dữ liệu.\n");

    return EXIT_SUCCESS;
}
