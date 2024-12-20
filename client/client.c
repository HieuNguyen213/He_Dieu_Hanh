#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <time.h>

#define SERVER_ADDR "127.0.0.1"  // Địa chỉ IP của server
#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_PATH    BUFFER_SIZE
#define TRUE    1
#define FALSE   0

uint8_t folder_exist = TRUE;
uint8_t file_exist = TRUE;
uint8_t file_no_change = TRUE;

int is_directory(const char *path);
void receive_response(int client_fd);

typedef struct {
    char filename[1024];   // Tên tệp tin
    char filepath[1024];   // Đường dẫn đầy đủ đến tệp tin
    long filesize;         // Kích thước tệp tin
    unsigned char hash[16]; // Hash của tệp tin (MD5)
    time_t timestamp;
} FileInfo;

//Hàm để nhận và in ra cây thư mục trên server
void receive_and_print_tree(int sock) {
    char buffer[2048];
    
    // Nhận dữ liệu từ server và hiển thị
    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Xóa nội dung buffer
        int bytes_read = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) {
            // Kết thúc khi không còn dữ liệu
            break;
        }
        printf("%s", buffer); // Hiển thị dữ liệu
    }
}

int check_directory_exists(const char *dir_path) {
    struct stat info;

    // Kiểm tra xem thư mục có tồn tại hay không
    if (stat(dir_path, &info) != 0) {
        // Thư mục không tồn tại hoặc không thể truy cập
        return 0; // False
    } else if (S_ISDIR(info.st_mode)) {
        // Đây là một thư mục
        return 1; // True
    } else {
        // Đây không phải là thư mục (có thể là file thông thường)
        return 0; // False
    }
}

// Hàm để lấy timestamp (thời gian sửa đổi) của một file
time_t get_file_timestamp(const char *filename) {
    struct stat file_info;

    // Lấy thông tin về file
    if (stat(filename, &file_info) == 0) {
        return file_info.st_mtime;  // Trả về thời gian sửa đổi của file
    } else {
        perror("Không thể lấy thông tin file");
        return -1;  // Trả về -1 nếu có lỗi
    }
}

void send_file_count(int client_fd, int file_count) {
    // Chuyển đổi thứ tự byte nếu cần (network byte order)
    int network_file_count = htonl(file_count);

    // Gửi dữ liệu
    if (send(client_fd, &network_file_count, sizeof(network_file_count), 0) < 0) {
        perror("Gửi file_count thất bại");
    } else {
        printf("Đã gửi file_count: %d\n", file_count);
    }
}


// Hàm để lọc ra phần đường dẫn khác nhau giữa 2 đường dẫn (bỏ qua tên file)
void get_different_path(const char *base_path, const char *file_path, char *result) {
    // Tìm vị trí của tên file trong file_path
    const char *last_slash = strrchr(file_path, '/');
    if (!last_slash) {
        // Nếu không tìm thấy '/', không thể lấy đường dẫn
        result[0] = '\0';
        return;
    }

    // Sao chép phần đường dẫn (bỏ qua tên file)
    char file_dir[MAX_PATH];
    strncpy(file_dir, file_path, last_slash - file_path + 1);
    file_dir[last_slash - file_path + 1] = '\0';

    // So sánh base_path với file_dir để tìm phần khác biệt
    const char *base_ptr = base_path;
    const char *file_ptr = file_dir;

    // Bỏ qua các ký tự giống nhau ban đầu
    while (*base_ptr && *file_ptr && *base_ptr == *file_ptr) {
        base_ptr++;
        file_ptr++;
    }

    // Copy phần còn lại của file_dir (phần khác biệt)
    if (*base_ptr == '\0' && *file_ptr == '/') {
        strcpy(result, file_ptr); // Copy phần còn lại từ file_dir
    } else {
        result[0] = '\0'; // Nếu không có phần khác biệt
    }
}

void send_file(int socket_fd, const char *path_file, const char *file_path) {
    printf("path file: %s\n", path_file);
    char buffer[BUFFER_SIZE];
    FILE *file = fopen(path_file, "rb");
    if (!file) {
        perror("Lỗi mở file");
        exit(EXIT_FAILURE);
    }

    // Tách tên file từ đường dẫn
    const char *file_name = strrchr(path_file, '/');
    if (file_name) {
        file_name++; // Bỏ qua dấu '/' để lấy tên file
    } else {
        file_name = path_file;
    }

    // Gửi đường dẫn thư mục và tên file
    printf("file path: %s\n", file_path);
    if (send(socket_fd, file_path, strlen(file_path) + 1, 0) == -1) {
        perror("Lỗi gửi đường dẫn file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    printf("file name: %s\n", file_name);
    if (send(socket_fd, file_name, strlen(file_name) + 1, 0) < 0) {
        perror("Lỗi gửi tên file");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    receive_response(socket_fd);

    // Tính toán và gửi kích thước file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    printf("Kích thước file: %ld bytes\n", file_size);
    if (send(socket_fd, &file_size, sizeof(file_size), 0) < 0) {
        perror("Lỗi gửi kích thước file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Gửi nội dung file
    char buffer2[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = fread(buffer2, 1, BUFFER_SIZE, file)) > 0) {
        if (send(socket_fd, buffer2, bytes_read, 0) < 0) {
            perror("Failed to send file");
            fclose(file);
            return;
        }
    }
    fclose(file);
    printf("File đã được gửi thành công.\n");
}


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

    // So sánh với chuỗi "folder no exists"
    if (strcmp(buffer, "folder no exists") == 0) {
        folder_exist = FALSE;
    }
    else if(strcmp(buffer, "folder exists") == 0){
        folder_exist = TRUE;
    }
    else if(strcmp(buffer, "File no exist") == 0)
    {
        printf("Server_File no exist\n");
        file_exist = FALSE;
    }
    else if(strcmp(buffer, "File exist") == 0)
    {
        printf("Server_File exist\n");
        file_exist = TRUE;
    }
    else if(strcmp(buffer, "File change") == 0)
    {
        printf("Server_File change!\n");
        file_no_change = FALSE;
    }
    else if(strcmp(buffer, "File no change") == 0)
    {
        printf("Server_File no change!\n");
        file_no_change = TRUE;
    }
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
                    file_list[*file_count].timestamp = get_file_timestamp(full_path);
                    (*file_count)++;
                }
            }
        }
    }

    closedir(dir);  // Đóng thư mục sau khi duyệt xong
}

// Hàm gửi thông tin file tới server
void send_file_info(int client_fd, FileInfo *file_info) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Gửi tên file
    snprintf(buffer, sizeof(buffer), "%s", file_info->filename);
    send(client_fd, buffer, strlen(buffer) + 1, 0);  // Gửi tên file
    printf("Đã gửi tên file: %s\n", file_info->filename);

    // Đợi phản hồi từ server
    char response[BUFFER_SIZE];
    memset(response, 0, sizeof(response));
    recv(client_fd, response, sizeof(response), 0);
    printf("Phản hồi từ server: %s\n", response);

    printf("\n");
    // Gửi đường dẫn file
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", file_info->filepath);
    send(client_fd, buffer, strlen(buffer) + 1, 0);  // Gửi đường dẫn file
    printf("Đã gửi đường dẫn: %s\n", file_info->filepath);

    printf("\n");
    // Đợi phản hồi từ server
    memset(response, 0, sizeof(response));
    recv(client_fd, response, sizeof(response), 0);
    printf("Phản hồi từ server: %s\n", response);

    printf("\n");
    // Gửi kích thước file
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%ld bytes", file_info->filesize);
    send(client_fd, buffer, strlen(buffer) + 1, 0);  // Gửi kích thước file
    printf("Đã gửi kích thước file: %ld bytes\n", file_info->filesize);

    // Đợi phản hồi từ server
    memset(response, 0, sizeof(response));
    recv(client_fd, response, sizeof(response), 0);
    printf("Phản hồi từ server: %s\n", response);

    // Gửi hash của file dưới dạng hex
    // Khởi tạo buffer
    char hash_buffer[65]; // 32 bytes * 2 ký tự cho mỗi byte + 1 ký tự null terminator
    hash_buffer[0] = '\0'; // Đảm bảo chuỗi bắt đầu trống

    // Ghép các giá trị hash vào buffer
    for (int i = 0; i < 16; i++) {
        snprintf(hash_buffer + strlen(hash_buffer), sizeof(hash_buffer) - strlen(hash_buffer), "%02x", file_info->hash[i]);
    }

    // Gửi toàn bộ hash dưới dạng hex
    send(client_fd, hash_buffer, strlen(hash_buffer) + 1, 0);  // Gửi toàn bộ hash
    printf("Đã gửi hash: %s\n", hash_buffer);

    // Đợi phản hồi từ server sau khi gửi hết hash
    memset(response, 0, sizeof(response));
    recv(client_fd, response, sizeof(response), 0);
    printf("Phản hồi từ server: %s\n", response);
}

// Hàm thực thi các tùy chọn
void handle_option(int client_fd) {
    char input[1024];
    char command[16];         // Lệnh đầu tiên (dự kiến là "rsync")
    char message[BUFFER_SIZE];
    char dir_path[1024];
    char dir_path_server[1024];
    FileInfo file_list[1000];  // Mảng lưu thông tin tệp tin
    int file_count = 0;

    printf("Nhập chuỗi (rsync <thư mục nguồn> <thư mục đích>):\n");
    fgets(input, sizeof(input), stdin);  // Đọc chuỗi từ người dùng
    input[strcspn(input, "\n")] = '\0';  // Loại bỏ ký tự xuống dòng nếu có

    if (sscanf(input, "%15s %127s %127s", command, dir_path, dir_path_server) == 3)
    {
        if (strcmp(command, "rsync") == 0)
        {
            //Gửi command đến cho server
            send(client_fd, command, strlen(command), 0);
            printf("Đã gửi command đến server: %s\n", command);
            receive_response(client_fd);

            // Gửi đường dẫn thư mục đến server
            send(client_fd, dir_path, strlen(dir_path), 0);
            printf("Đã gửi đường dẫn thư mục đến server: %s\n", dir_path);
            receive_response(client_fd);

            // Gửi đường dẫn thư mục sẽ lưu trên server
            send(client_fd, dir_path_server, strlen(dir_path_server), 0);
            printf("Đã gửi đường dẫn thư mục đến server: %s\n", dir_path_server);
            receive_response(client_fd);

            receive_response(client_fd);

            // Liệt kê các tệp tin và lưu thông tin
            list_files(dir_path, file_list, &file_count);

            // In ra thông tin của các tệp tin
            for (int i = 0; i < file_count; i++) {
                printf("Tệp tin: %s\n", file_list[i].filename);
                printf("Đường dẫn: %s\n", file_list[i].filepath);
                printf("Kích thước: %ld bytes\n", file_list[i].filesize);
                struct tm *time_info = localtime(&file_list[i].timestamp);
                printf("Thời gian sửa đổi của file là: %s", asctime(time_info));
                printf("Hash (MD5): ");
                for (int j = 0; j < 16; j++) {
                    printf("%02x", file_list[i].hash[j]);
                }
                printf("\n\n");
            }

            if(folder_exist == FALSE)
            {
                //printf("Send all because folder no exists!");
                folder_exist = TRUE;
                for (int i = 0; i < file_count; i++)
                {
                    char different_path[MAX_PATH];
                    // Lọc phần đường dẫn khác nhau
                    printf("dir path: %s\n", dir_path);
                    printf("file path: %s\n", file_list[i].filepath);
                    get_different_path(dir_path, file_list[i].filepath, different_path);
                    printf("different path: %s\n", different_path);
                    send_file(client_fd, file_list[i].filepath, different_path);
                    receive_response(client_fd);
                }
            }
            else
            {
                printf("Folder tồn tại rồi, khó xử lý hơn đấy!\n");

                //Đầu tiên phải gửi file count đến cho server
                send_file_count(client_fd, file_count);
                printf("Đã gửi file count %d\n", file_count);
                receive_response(client_fd);

                //Gửi thông tin của các tệp tin cho server
                for (int i = 0; i < file_count; i++) {
                    char temp[256];
                    strcpy(temp, file_list[i].filepath);
                    printf("temp1: %s\n", temp);
                    char different_path[MAX_PATH];
                    get_different_path(dir_path, file_list[i].filepath, different_path);
                    printf("different path: %s\n", different_path);
                    strcpy(file_list[i].filepath, different_path);
                    send_file_info(client_fd, &file_list[i]);
                    receive_response(client_fd); // Nhận phản hồi từ server sau mỗi tệp tin

                    receive_response(client_fd); //nhận phản hồi từ server để kiểm tra file có tồn tại hay không
                    
                    if(file_exist == FALSE)
                    {
                        printf("gửi thông tin file!\n");
                        file_exist = TRUE;
                        printf("temp2: %s\n", temp);
                        send_file(client_fd, temp, different_path);
                        receive_response(client_fd);
                    }

                    else
                    {
                        receive_response(client_fd);
                        if(file_no_change == FALSE)
                        {
                            send_file(client_fd, temp, different_path);
                            receive_response(client_fd);
                        }
                    }
                    printf("\n\n");
                }
            }
            printf("\n");
        }

        else if(strcmp(command, "clone") == 0)
        {
            printf("Đồng bộ từ server về client!\n");
            printf("Thư mục muốn đồng bộ ở server: %s\n", dir_path);
            printf("Thư mục sẽ lưu ở client: %s\n", dir_path_server);

            send(client_fd, command, strlen(command), 0);
            printf("Đã gửi command đến server: %s\n", command);
            receive_response(client_fd);

            // Gửi đường dẫn thư mục cần đồng bộ trên server
            send(client_fd, dir_path, strlen(dir_path), 0);
            printf("Đã gửi đường dẫn thư mục cần đồng bộ trên server: %s\n", dir_path);
            receive_response(client_fd);

            if (check_directory_exists(dir_path_server)) {
                printf("Thư mục tồn tại.\n");
            } else {
                printf("Thư mục không tồn tại.\n");
            }

        }

        else
        {
            return;
        }
    }

    else if (sscanf(input, "%15s %127s %127s", command, dir_path, dir_path_server) == 1)
    {
        if (strcmp(command, "ls") == 0)
        {
            //Gửi command đến cho server
            send(client_fd, command, strlen(command), 0);
            printf("Đã gửi command đến server: %s\n", command);
            receive_response(client_fd);

            receive_and_print_tree(client_fd);

            printf("OK, đã in ra màn hình!\n");
        }
    }

    else
    {
        return;
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
        //show_menu();
        //scanf("%d", &option);
        //handle_option(client_fd, option);
        handle_option(client_fd);
    }

    return 0;
}