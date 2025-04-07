#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define FILE_PATH "/dev/my_mouse"
#define OUTPUT_FILE "/home/nhatphap/Desktop/mouse_data_full.txt"
#define MAX_LINES 100  // Giới hạn số dòng trong file
#define LINE_LENGTH 512

// Biến toàn cục để theo dõi tọa độ chuột cũ và thời gian cũ
int last_x = 0, last_y = 0;
time_t last_time = 0;

// Biến theo dõi số lần không thay đổi tọa độ và tổng số lần đọc
int unchanged_position_count = 0;
int total_read_count = 0;

// Hàm tính tốc độ chuột
double calculate_speed(int x, int y, time_t current_time) {
    double delta_t = difftime(current_time, last_time);  // giây
    if (delta_t == 0) return 0;

    int delta_x = x - last_x;
    int delta_y = y - last_y;
    return sqrt(delta_x * delta_x + delta_y * delta_y) / delta_t;
}

// Hàm tính độ chính xác (accuracy)
double calculate_accuracy() {
    if (total_read_count == 0) return 0.0;  // Tránh chia cho 0
    return (double)unchanged_position_count / total_read_count;
}

// Hàm ghi dữ liệu JSON vào file, cập nhật dữ liệu cũ nếu cần
void write_data_to_file(int x, int y, int abs_x, int abs_y,
                        int left_clicks, int right_clicks, int wheel_clicks,
                        double speed, double accuracy) {
    FILE *output_fp;
    char buffer[MAX_LINES][LINE_LENGTH];  // Lưu trữ các dòng dữ liệu hiện có
    int line_count = 0;

    // Mở file và đọc dữ liệu hiện có
    output_fp = fopen(OUTPUT_FILE, "r");
    if (output_fp != NULL) {
        // Đọc các dòng cũ từ file
        while (fgets(buffer[line_count], LINE_LENGTH, output_fp) != NULL && line_count < MAX_LINES) {
            line_count++;
        }
        fclose(output_fp);
    }

    // Mở lại file để ghi dữ liệu mới
    output_fp = fopen(OUTPUT_FILE, "w");
    if (output_fp == NULL) {
        perror("❌ Không thể mở file output");
        return;
    }

    // Thêm dữ liệu mới vào đầu file
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_buffer[100];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", t);

    fprintf(output_fp,
        "{\"timestamp\": \"%s\", \"x\": %d, \"y\": %d, \"abs_x\": %d, \"abs_y\": %d, "
        "\"left_clicks\": %d, \"right_clicks\": %d, \"wheel_clicks\": %d, \"speed\": %.2f, \"accuracy\": %.2f}\n",
        time_buffer, x, y, abs_x, abs_y, left_clicks, right_clicks, wheel_clicks, speed, accuracy);

    // Ghi lại các dòng cũ, nếu không vượt quá giới hạn
    for (int i = 0; i < line_count && i < MAX_LINES - 1; i++) {
        fprintf(output_fp, "%s", buffer[i]);
    }

    fclose(output_fp);
    printf("✅ Ghi JSON: total click(L+R) = %d, wheel_clicks = %d, SPEED=%.2f at position(x,y) (%d, %d), ACCURACY=%.2f\n", 
           left_clicks + right_clicks, wheel_clicks, speed, abs_x, abs_y, accuracy);
}

// Hàm đọc từ /dev/my_mouse và xử lý
void read_and_save_data_to_file() {
    FILE *fp = fopen(FILE_PATH, "r");
    if (fp == NULL) {
        perror("❌ Không thể mở /dev/my_mouse");
        return;
    }

    char data[256];
    if (fgets(data, sizeof(data), fp) != NULL) {
        int x, y, abs_x, abs_y, left_clicks, right_clicks, wheel_clicks;

        if (sscanf(data, "{\"x\": %d, \"y\": %d, \"abs_x\": %d, \"abs_y\": %d, \"left_clicks\": %d, \"right_clicks\": %d, \"wheel_clicks\": %d}",
                   &x, &y, &abs_x, &abs_y, &left_clicks, &right_clicks, &wheel_clicks) == 7) {
            time_t current_time = time(NULL);
            double speed = calculate_speed(x, y, current_time);

            // Tính độ chính xác
            if (x == last_x && y == last_y) {
                unchanged_position_count++;
            }

            total_read_count++;

            double accuracy = calculate_accuracy();

            write_data_to_file(x, y, abs_x, abs_y, left_clicks, right_clicks, wheel_clicks, speed, accuracy);

            last_x = x;
            last_y = y;
            last_time = current_time;
        } else {
            printf("⚠️ Dữ liệu không hợp lệ: %s\n", data);
        }
    } else {
        perror("❌ Không đọc được dữ liệu từ /dev/my_mouse");
    }

    fclose(fp);
}

int main() {
    while (1) {
        read_and_save_data_to_file();
        usleep(500000);  // 500ms
    }

    return 0;
}
