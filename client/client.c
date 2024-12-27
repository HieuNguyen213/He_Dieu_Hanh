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
#include <errno.h>  

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

int create_directory_recursively(const char *path) {
    char temp_path[512];
    char *p = NULL;
    size_t len;

    // Sao chép đường dẫn vào biến tạm
    snprintf(temp_path, sizeof(temp_path), "%s", path);

    // Lặp qua các phần của đường dẫn và tạo từng thư mục một
    len = strlen(temp_path);
    if (temp_path[len - 1] == '/') {
        temp_path[len - 1] = '\0'; // Xóa ký tự '/' cuối nếu có
    }

    // Tạo từng thư mục một
    for (p = temp_path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0'; // Tạm thời thay '/' thành '\0' để tạo thư mục
            if (mkdir(temp_path, 0777) == -1 && errno != EEXIST) {
                perror("Lỗi tạo thư mục");
                return -1;
            }
            *p = '/'; // Khôi phục lại dấu '/'
        }
    }

    // Tạo thư mục cuối cùng
    if (mkdir(temp_path, 0777) == -1 && errno != EEXIST) {
        perror("Lỗi tạo thư mục");
        return -1;
    }

    return 0;
}

// Hàm nhận thông tin file từ server và lưu vào file_info
void receive_file_info(int client_fd, FileInfo *file_info) {
    char buffer[BUFFER_SIZE];

    // Nhận tên file
    //memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Nhận tên file: %s\n", buffer);
    strncpy(file_info->filename, buffer, sizeof(file_info->filename) - 1);  // Lưu tên file vào struct
    // Phản hồi cho client
    const char *sp1 = "Receive file name ok!";
    send_response(client_fd, sp1);

    // Nhận đường dẫn file
    //memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Nhận đường dẫn file: %s\n", buffer);
    strncpy(file_info->filepath, buffer, sizeof(file_info->filepath) - 1);  // Lưu đường dẫn vào struct
    // Phản hồi cho client
    const char *sp2 = "Receive path file ok!";
    send_response(client_fd, sp2);

    // Nhận kích thước file
    //memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Nhận kích thước file: %s\n", buffer);
    file_info->filesize = atol(buffer);  // Lưu kích thước file vào struct
    const char *sp3 = "Receive file size ok!";
    send_response(client_fd, sp3);

    // Nhận hash của file
    //memset(buffer, 0, sizeof(buffer));
    recv(client_fd, buffer, sizeof(buffer), 0);
    printf("Nhận hash: %s\n", buffer);

    // Xử lý và lưu hash vào struct
    int hash_index = 0;
    while (hash_index < 32 && sscanf(buffer + hash_index * 2, "%02hhx", &file_info->hash[hash_index]) == 1) {
        hash_index++;
    }

    // Phản hồi cho client
    const char *sp4 = "Receive hash file ok!";
    send_response(client_fd, sp4);
}

//hàm nhận file count
void receive_file_count(int client_fd, int *file_count) {
    int network_file_count;

    // Nhận dữ liệu
    if (recv(client_fd, &network_file_count, sizeof(network_file_count), 0) > 0) {
        // Chuyển đổi thứ tự byte từ network byte order sang host byte order
        *file_count = ntohl(network_file_count);
        printf("Nhận file_count: %d\n", *file_count);
    } else {
        perror("Nhận file_count thất bại");
    }
}

//Hàm nhận file
void receive_file(int socket_fd, const char *base_path) {
    char path_buffer[BUFFER_SIZE];
    char file_name[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Nhận đường dẫn thư mục từ server
    ssize_t path_len = recv(socket_fd, path_buffer, BUFFER_SIZE, 0);

    if (path_len == -1) {
        perror("Lỗi nhận đường dẫn");
        return;
    }
    path_buffer[path_len] = '\0';

    printf("path_buffer: %s\n", path_buffer);

    // Nhận tên file từ client
    ssize_t file_name_len = recv(socket_fd, file_name, BUFFER_SIZE, 0);
    if (file_name_len == -1) {
        perror("Lỗi nhận tên file");
        return;
    }
    file_name[file_name_len] = '\0';  // Đảm bảo kết thúc chuỗi
    const char *response_message = "receive file name successful";
    send_response(socket_fd, response_message);
    printf("file name: %s\n", file_name);

    // Tạo đường dẫn đầy đủ cho file (bao gồm thư mục base_path, đường dẫn thư mục từ client và tên file)
    char full_file_path[BUFFER_SIZE];

    if(path_len == 1)
    {
        snprintf(full_file_path, sizeof(full_file_path), "%s", base_path);
    }
    else{
        snprintf(full_file_path, sizeof(full_file_path), "%s%s", base_path, path_buffer);
    }
    printf("đường dẫn thư mục tạo để ghi: %s\n", full_file_path);

    if (create_directory_recursively(full_file_path) == 0) {
        printf("Các thư mục đã được tạo thành công.\n");
    } else {
        printf("Có lỗi khi tạo thư mục.\n");
    }

    char full_file_path2[BUFFER_SIZE];
    snprintf(full_file_path2, sizeof(full_file_path2), "%s%s", full_file_path, file_name);

    printf("file name: %s\n", file_name);
    printf("đường dẫn file để ghi là: %s\n", full_file_path2);

    // Nhận kích thước file
    long file_size;
    if (recv(socket_fd, &file_size, sizeof(file_size), 0) <= 0) {
        perror("Lỗi nhận kích thước file");
        return;
    }
    printf("Kích thước file: %ld bytes\n", file_size);

    // Mở file để ghi dữ liệu nhận được
    int bytes_received;
    FILE *file = fopen(full_file_path2, "wb");
    if (!file) {
        perror("Lỗi mở file để ghi");
        return;
    }
    printf("Receiving file: %s\n", file_name);

    char buffer2[BUFFER_SIZE];
    long total_received = 0;
    
    while (total_received < file_size) {
        ssize_t bytes_received = recv(socket_fd, buffer2, sizeof(buffer2), 0);
        if (bytes_received <= 0) {
            perror("Lỗi nhận dữ liệu file");
            fclose(file);
            return;
        }
        fwrite(buffer2, 1, bytes_received, file);
        total_received += bytes_received;
    }

    fclose(file);
    printf("File '%s' received successfully!\n", file_name);
}

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

// Hàm kiểm tra xem đường dẫn có tồn tại không
int check_path_exists(const char *base_path, const char *relative_path) {
    char full_path[1024];
    struct stat path_stat;

    // Kết hợp đường dẫn cơ sở với đường dẫn tương đối
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, relative_path);

    // Kiểm tra sự tồn tại của đường dẫn
    if (stat(full_path, &path_stat) == 0) {
        // Đường dẫn tồn tại
        return 1;
    } else {
        // Đường dẫn không tồn tại
        perror("Lỗi kiểm tra đường dẫn");
        return 0;
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

//Hàm gửi phàn hồi đến server
// Hàm gửi response từ server về client
void send_response(int client_socket, const char *message) {
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("Failed to send response");
    } else {
        printf("Response sent to server: %s\n", message);
    }
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
    int file_count = 0;

    printf("Nhập chuỗi (rsync <thư mục nguồn> <thư mục đích>):\n");
    fgets(input, sizeof(input), stdin);  // Đọc chuỗi từ người dùng
    input[strcspn(input, "\n")] = '\0';  // Loại bỏ ký tự xuống dòng nếu có

    if (sscanf(input, "%15s %127s %127s", command, dir_path, dir_path_server) == 3)
    {
        if (strcmp(command, "rsync") == 0)
        {
            FileInfo file_list[1000];  // Mảng lưu thông tin tệp tin
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
                folder_exist = FALSE;
            }
            printf("\n");
        }

        else if(strcmp(command, "clone") == 0)
        {
            FileInfo file_list_in_client[1000];  // Mảng lưu thông tin tệp tin
            int temp_file_count;
            printf("Đồng bộ từ server về client!\n");
            printf("Thư mục muốn đồng bộ ở server: %s\n", dir_path);
            printf("Thư mục sẽ lưu ở client: %s\n", dir_path_server);

            //Lấy thông tin các file trong thư mục lưu ở client
            list_files(dir_path_server, file_list_in_client, &temp_file_count);

            send(client_fd, command, strlen(command), 0);
            printf("Đã gửi command đến server: %s\n", command);
            receive_response(client_fd);

            // Gửi đường dẫn thư mục cần đồng bộ trên server
            send(client_fd, dir_path, strlen(dir_path), 0);
            printf("Đã gửi đường dẫn thư mục cần đồng bộ trên server: %s\n", dir_path);
            receive_response(client_fd);

            receive_response(client_fd);

            //kiểm tra xem folder cần tải về có tồn tại trên server hay không
            if(folder_exist == TRUE)
            {   
                FileInfo file_info_from_server[1000];
                if (check_directory_exists(dir_path_server)) 
                {
                    printf("Thư mục lưu ở client tồn tại.\n");
                    const char *response = "folder in client exists";
                    send_response(client_fd, response);
                    
                    //đầu tiên cần nhận file count: số lượng file sẽ đồng bộ trên server
                    int file_count_from_server;
                    receive_file_count(client_fd, &file_count_from_server); 
                    
                    //Thực hiện nhận thông tin file
                    char temp[MAX_PATH];
                    for(int i = 0; i < file_count_from_server; i++)
                    {
                        memset(temp, 0, sizeof(temp));
                        receive_file_info(client_fd, &file_info_from_server[i]);
                        printf("/n");

                        // Kiểm tra xem ký tự cuối cùng có phải là '/' không
                        if (file_info_from_server[i].filepath[strlen(file_info_from_server[i].filepath) - 1] == '/') {
                            snprintf(temp, MAX_PATH, "%s/%s%s", dir_path_server, file_info_from_server[i].filepath, file_info_from_server[i].filename);
                        } 
                        else 
                        {
                            snprintf(temp, MAX_PATH, "%s/%s/%s", dir_path_server, file_info_from_server[i].filepath, file_info_from_server[i].filename);
                        }

                        printf("temp: %s\n", temp);

                        // Kiểm tra xem đường dẫn có tồn tại không
                        struct stat path_stat;

                        if (stat(temp, &path_stat) == 0) {
                            printf("Đường dẫn tồn tại.\n");
                            const char *response2 = "File exists";
                            send_response(client_fd, response2);

                            char full_path_client[256]; 
                            snprintf(full_path_client, sizeof(full_path_client), "%s/%s", dir_path_server, dir_path);
                            for(int index = 0; index < temp_file_count; index++)
                            {
                                if (strcmp(file_list_in_client[index].filename, file_info_from_server[i].filename) == 0) {
                                    // Hai tên file giống nhau
                                    if (!(memcmp(file_list_in_client[index].hash, file_info_from_server[i].hash, sizeof(file_list_in_client[index].hash)) == 0))
                                    {
                                        printf("\n");
                                        if(file_list_in_client[index].timestamp > file_info_from_server[i].timestamp)
                                        {
                                            char select;
                                            printf("File %s on the client has been modified. \nDo you want to sync that file from the server?[Y/N]\n", file_list_in_client[index].filename);
                                            
                                            while ((select = getchar()) == '\n');
                                            printf("select: %c\n", select);
                                            if(select == 'n' || select == 'N')
                                            {
                                                const char *rpp = "File no change";
                                                send_response(client_fd, rpp);
                                            }
                                            else if(select == 'y' || select == 'Y')
                                            {
                                                const char *rpp = "File change";
                                                send_response(client_fd, rpp);
                                                receive_file(client_fd, full_path_client);
                                                const char *send_file_message = "File received successfully.";
                                                send_response(client_fd, send_file_message);
                                                printf("File thay đổi là: %s\n", file_list_in_client[index].filename);
                                            }
                                        }
                                        break;
                                    }

                                    else
                                    {
                                        printf("File %s không thay đổi\n", file_list_in_client[index].filename);
                                        const char *response = "File no change";
                                        send_response(client_fd, response);
                                        break;
                                    }
                                }
                            }
                        } 
                        else 
                        {
                            printf("Đường dẫn không tồn tại.\n");
                            const char *response = "File no exists";
                            send_response(client_fd, response);

                            //Nhận file
                            char full_path_client[256]; 
                            snprintf(full_path_client, sizeof(full_path_client), "%s/%s", dir_path_server, dir_path);
                            receive_file(client_fd, full_path_client);
                            const char *rp1 = "File received successfully.";
                            send_response(client_fd, rp1);
                            printf("\n");
                        }
                    }
                } 

                else 
                {
                    printf("Thư mục lưu ở client không tồn tại.\n");
                    //tại đây thì cứ tiến hành tải full thư mục từ server xuống thôi
                    const char *response = "folder in client no exists";
                    send_response(client_fd, response);

                    //Nhận file
                    char full_path_client[256]; 
                    snprintf(full_path_client, sizeof(full_path_client), "%s/%s", dir_path_server, dir_path);
                    while(client_fd >= 0)
                    {
                        receive_file(client_fd, full_path_client);
                        const char *response = "File received successfully.";
                        send_response(client_fd, response);
                    }
                }

                folder_exist = FALSE;
            }

            else
            {
                printf("Thư mục cần tải trên server không tồn tại.\n");
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
        getchar();
    }

    return 0;
}