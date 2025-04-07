#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include "MQTTClient.h"
#include <cjson/cJSON.h>

// Thông tin MQTT
#define ADDRESS     "ssl://05e0816ba798455ab55c337172ebb78e.s1.eu.hivemq.cloud:8883"
#define CLIENTID    "PhapSub"
#define TOPIC       "PHAP/MOUSE_DATA"
#define USERNAME    "nhatphap_mqtt"
#define PASSWORD    "Phap123456"
#define QOS         1
#define TIMEOUT     10000L

// Thông tin MySQL
#define DB_HOST     "localhost"
#define DB_USER     "phap_1"
#define DB_PASS     "phap"  
#define DB_NAME     "mouse_data"

MYSQL *conn;

// Hàm chèn dữ liệu vào MySQL
void insert_into_mysql(const char *payload) {
    cJSON *json = cJSON_Parse(payload);
    if (!json) {
        fprintf(stderr, "❌ JSON parse lỗi\n");
        return;
    }

    // Lấy các trường dữ liệu từ JSON
    const char *timestamp = cJSON_GetObjectItem(json, "timestamp")->valuestring;
    int x = cJSON_GetObjectItem(json, "x")->valueint;
    int y = cJSON_GetObjectItem(json, "y")->valueint;
    int abs_x = cJSON_GetObjectItem(json, "abs_x")->valueint;
    int abs_y = cJSON_GetObjectItem(json, "abs_y")->valueint;
    int left_clicks = cJSON_GetObjectItem(json, "left_clicks")->valueint;
    int right_clicks = cJSON_GetObjectItem(json, "right_clicks")->valueint;
    int wheel_clicks = cJSON_GetObjectItem(json, "wheel_clicks")->valueint;
    double speed = cJSON_GetObjectItem(json, "speed")->valuedouble;
    double accuracy = cJSON_GetObjectItem(json, "accuracy")->valuedouble;

    // Chuẩn bị câu lệnh SQL để chèn dữ liệu vào bảng
    char query[1024];
    snprintf(query, sizeof(query),
             "INSERT INTO mouse_events (timestamp, x, y, abs_x, abs_y, left_clicks, right_clicks, wheel_clicks, speed, accuracy) "
             "VALUES ('%s', %d, %d, %d, %d, %d, %d, %d, %.2f, %.2f);",
             timestamp, x, y, abs_x, abs_y, left_clicks, right_clicks, wheel_clicks, speed, accuracy);

    // Thực thi câu lệnh SQL
    if (mysql_query(conn, query)) {
        fprintf(stderr, "❌ MySQL insert lỗi: %s\n", mysql_error(conn));
    } else {
        printf("✅ Đã lưu vào MySQL: %s\n", payload);
    }

    // Xoá đối tượng JSON sau khi xử lý
    cJSON_Delete(json);
}

// Callback khi nhận được tin nhắn MQTT
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *payload = (char *)message->payload;
    char data[message->payloadlen + 1];
    memcpy(data, payload, message->payloadlen);
    data[message->payloadlen] = '\0';

    printf("📩 Nhận dữ liệu: %s\n", data);
    insert_into_mysql(data);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main() {
    // Kết nối MySQL
    conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0)) {
        fprintf(stderr, "❌ Lỗi kết nối MySQL: %s\n", mysql_error(conn));
        return EXIT_FAILURE;
    }
    printf("🛢️  Đã kết nối MySQL thành công.\n");

    // MQTT setup
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL);

    ssl_opts.enableServerCertAuth = 1;

    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.username = USERNAME;
    conn_opts.password = PASSWORD;
    conn_opts.ssl = &ssl_opts;

    printf("🔌 Đang kết nối MQTT...\n");
    int rc = MQTTClient_connect(client, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "❌ Kết nối MQTT thất bại: %s\n", MQTTClient_strerror(rc));
        return EXIT_FAILURE;
    }
    printf("✅ Kết nối MQTT thành công.\n");

    MQTTClient_subscribe(client, TOPIC, QOS);
    printf("📡 Đang chờ dữ liệu từ topic: %s\n", TOPIC);

    while (1) {
        sleep(1);
    }

    // Cleanup
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    mysql_close(conn);
    return EXIT_SUCCESS;
}
