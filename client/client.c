#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#define SERVER_ADDR "127.0.0.1"  // Địa chỉ IP của server
#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct {
    char filename[1024];   // Tên tệp tin
    char filepath[1024];   // Đường dẫn đầy đủ đến tệp tin
    long filesize;         // Kích thước tệp tin
    unsigned char hash[16]; // Hash của tệp tin (MD5)
} FileInfo;

int calculate_file_hash(const char *filepath, unsigned char *hash_out) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Không thể mở tệp tin");
        return -1;
    }

    SHA256_CTX sha256Context;
    unsigned char data[1024];
    size_t bytesRead;

    SHA256_Init(&sha256Context);
    
    while ((bytesRead = fread(data, 1, sizeof(data), file)) > 0) {
        SHA256_Update(&sha256Context, data, bytesRead);
    }

    SHA256_Final(hash_out, &sha256Context);

    fclose(file);
    return 0;
}


// Hàm tạo socket
int create_client_socket() {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return client_fd;
}

// Hàm kết nối đến server
void connect_to_server(int client_fd, struct sockaddr_in *server_addr) {
    if (connect(client_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Connection failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
}

// Hàm gửi yêu cầu đến server
void send_request(int client_fd, const char *message) {
    send(client_fd, message, strlen(message), 0);
}

// Hàm nhận phản hồi từ server
void receive_response(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("Server disconnected.\n");
        } else {
            perror("Error receiving data");
        }
        close(client_fd);  // Đóng kết nối khi server ngắt kết nối
        exit(EXIT_FAILURE);
    }
    buffer[bytes_received] = '\0';  // Đảm bảo chuỗi kết thúc
    printf("Received from server: %s\n", buffer);
}

// Hàm để kiểm tra xem một thư mục có phải là thư mục không
int is_directory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) == 0) {
        return S_ISDIR(statbuf.st_mode);  // Kiểm tra nếu là thư mục
    }
    return 0; // Không phải thư mục
}

// Hàm đệ quy liệt kê tất cả các tệp tin trong thư mục và các thư mục con
void list_files(const char *dir_path, FileInfo *file_list, int *file_count) {
    DIR *dir = opendir(dir_path);  // Mở thư mục
    struct dirent *entry;

    if (dir == NULL) {
        perror("Không thể mở thư mục");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[1024];
        
        // Bỏ qua thư mục '.' và '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Tạo đường dẫn đầy đủ của tệp hoặc thư mục
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // Nếu là thư mục, gọi đệ quy
                list_files(full_path, file_list, file_count); 
            } else {
                // Nếu là tệp tin, lưu thông tin tệp tin vào mảng
                strcpy(file_list[*file_count].filename, entry->d_name);
                strcpy(file_list[*file_count].filepath, full_path);
                file_list[*file_count].filesize = statbuf.st_size;

                // Tính hash cho tệp tin
                if (calculate_file_hash(full_path, file_list[*file_count].hash) == 0) {
                    (*file_count)++;
                }
            }
        }
    }

    closedir(dir);  // Đóng thư mục sau khi duyệt xong
}

// Hàm gửi thông tin tệp tin tới server
void send_file_info(int client_fd, FileInfo *file_info) {
    // Tạo chuỗi dữ liệu để gửi
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "Filename: %s\nFilepath: %s\nFilesize: %ld bytes\nHash: ",
             file_info->filename, file_info->filepath, file_info->filesize);

    // Gửi thông tin về tên, đường dẫn và kích thước tệp
    //send(client_fd, buffer, strlen(buffer), 0);

    // Gửi hash của tệp tin (dưới dạng hexa)
    for (int i = 0; i < 32; i++) {
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%02x", file_info->hash[i]);
    }
    send(client_fd, buffer, strlen(buffer), 0);
}

// Hàm hiển thị menu
void show_menu() {
    printf("\n----- Menu -----\n");
    printf("1. Gửi yêu cầu đến server\n");
    printf("2. Gửi yêu cầu khác\n");
    printf("0. Đóng kết nối\n");
    printf("Chọn một tùy chọn (0-2): ");
}

// Hàm thực thi các tùy chọn
void handle_option(int client_fd, int option) {
    char message[BUFFER_SIZE];
    char dir_path[1024];
    FileInfo file_list[1000];  // Mảng lưu thông tin tệp tin
    int file_count = 0;

    switch (option) {
        case 1:
            strcpy(message, "Hello from client! This is option 1.");
            send_request(client_fd, message);
            receive_response(client_fd);
            break;
        case 2:
            // strcpy(message, "This is another request from client, option 2.");
            // send_request(client_fd, message);
            // receive_response(client_fd);

            // Nhập đường dẫn thư mục cần đồng bộ và liệt kê tệp tin
            // printf("Nhập đường dẫn thư mục cần đồng bộ: ");
            // scanf("%s", dir_path);
            // list_files(dir_path);  // Liệt kê các tệp tin và thư mục con
            // break;

            // Nhập đường dẫn thư mục cần đồng bộ
            printf("Nhập đường dẫn thư mục cần đồng bộ: ");
            scanf("%s", dir_path);

            // Liệt kê các tệp tin và lưu thông tin
            list_files(dir_path, file_list, &file_count);

            // In ra thông tin của các tệp tin
            for (int i = 0; i < file_count; i++) {
                printf("Tệp tin: %s\n", file_list[i].filename);
                printf("Đường dẫn: %s\n", file_list[i].filepath);
                printf("Kích thước: %ld bytes\n", file_list[i].filesize);
                printf("Hash (MD5): ");
                for (int j = 0; j < 16; j++) {
                    printf("%02x", file_list[i].hash[j]);
                }
                printf("\n\n");
            }

            //Gửi thông tin của các tệp tin cho server
            for (int i = 0; i < file_count; i++) {
                printf("Đang gửi tệp tin: %s\n", file_list[i].filename);
                send_file_info(client_fd, &file_list[i]);
                receive_response(client_fd); // Nhận phản hồi từ server sau mỗi tệp tin
            }

            break;
        case 0:
            printf("Đóng kết nối và thoát.\n");
            close(client_fd);
            exit(0);  // Đóng kết nối và thoát chương trình
        default:
            printf("Lựa chọn không hợp lệ. Vui lòng chọn lại.\n");
    }
}

int main() {
    int client_fd, option;
    struct sockaddr_in server_addr;

    // Cấu hình địa chỉ server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);  // Địa chỉ IP của server

    // Tạo socket
    client_fd = create_client_socket();

    // Kết nối đến server
    connect_to_server(client_fd, &server_addr);

    // Menu chính
    while (1) {
        show_menu();
        scanf("%d", &option);
        handle_option(client_fd, option);
    }

    return 0;
}
