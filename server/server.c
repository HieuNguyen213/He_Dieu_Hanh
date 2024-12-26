#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>  
#include <dirent.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define STORAGE_PATH "../common/storage2"
#define HASH_SIZE 16

#define TRUE 1
#define FALSE 0

uint8_t folder_exist = TRUE;
uint8_t file_exist = TRUE;
uint8_t file_no_change = TRUE;
uint8_t folder_client_exist = TRUE;

typedef struct {
    char filename[1024];   // Tên tệp tin
    char filepath[1024];   // Đường dẫn đầy đủ đến tệp tin
    long filesize;         // Kích thước tệp tin
    unsigned char hash[16]; // Hash của tệp tin (MD5)
    time_t timestamp;
} FileInfo;

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

//hàm gửi file count
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

//Gửi cây thư mục từ server đến client
void print_tree(const char *path, int depth, int client_socket) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        perror("Không thể mở thư mục");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[1024];
        struct stat info;

        // Bỏ qua các mục đặc biệt "." và ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Tạo đường dẫn đầy đủ
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Lấy thông tin mục hiện tại
        if (stat(full_path, &info) == 0) {
            char buffer[2048];

            // Tạo chuỗi kết quả thụt lề theo độ sâu
            memset(buffer, 0, sizeof(buffer));
            for (int i = 0; i < depth; i++) {
                strcat(buffer, "│   ");
            }

            if (S_ISDIR(info.st_mode)) {
                // Nếu là thư mục
                snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "├── %s/\n", entry->d_name);
                send(client_socket, buffer, strlen(buffer), 0); // Gửi tới client
                print_tree(full_path, depth + 1, client_socket); // Đệ quy
            } else if (S_ISREG(info.st_mode)) {
                // Nếu là file
                snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "├── %s\n", entry->d_name);
                send(client_socket, buffer, strlen(buffer), 0); // Gửi tới client
            }
        }
    }
    closedir(dir);
}


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

int check_file_exists(const char *filepath) {
    if (access(filepath, F_OK) != -1) {
        // File tồn tại
        return 1;
    } else {
        // File không tồn tại
        return 0;
    }
}


//tạo thư mục lưu trữ trên máy server
void create_storage() 
{
    /* struct stat là 1 cấu trúc trong trong thư viện sys/stat.h dùng để lưu trữ thông tin về file hoặc thư mục.
    Cấu trúc này sẽ chứa thông tin về kích thước file, quyền truy cập, thời gian sửa đổi,... 
    Ban đầu thì ta sẽ khởi tạo mọi thành phần trong cấu trúc này là 0*/
    struct stat st = {0};

    /* Ta sẽ kiểm tra xem liệu storage đã được tạo hay chưa. hàm stat sẽ lấy thư mục STORAGE_PATH gán vào st.
    Nếu st == 0 tức là folder đã tồn tại và thông tin đã được lấy thành công. Ngược lại là chưa tồn tại */
    if (stat(STORAGE_PATH, &st) == -1) 
    {
        /* Chưa tồn tại thì tạo mới thư mục Storage.
        mkdir để tạo thư mục mới có tên như macro STORAGE_PATH.
         */
        if (mkdir(STORAGE_PATH, 0777) == 0) 
        {
            printf("Thư mục lưu trữ đã được tạo: %s\n", STORAGE_PATH);
        } 
        else 
        {
            perror("Lỗi khi tạo thư mục lưu trữ");
            exit(EXIT_FAILURE);  // Thoát chương trình nếu lỗi
        }
    } 
    else 
    {
        printf("Thư mục lưu trữ đã tồn tại: %s\n", STORAGE_PATH);
    }
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

// Hàm gửi response từ server về client
void send_response(int client_socket, const char *message) {
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("Failed to send response");
    } else {
        printf("Response sent to client: %s\n", message);
    }
}

// //Hàm nhận phản hồi từ client
void receive_response(int server_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(server_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("client disconnected.\n");
        } else {
            perror("Error receiving data");
        }
        close(server_fd);  // Đóng kết nối khi server ngắt kết nối
        exit(EXIT_FAILURE);
    }
    buffer[bytes_received] = '\0';  // Đảm bảo chuỗi kết thúc
    printf("Received from client: %s\n", buffer);

    // So sánh với chuỗi "folder no exists"
    if (strcmp(buffer, "folder in client no exists") == 0) {
        folder_client_exist = FALSE;
    }
    else if (strcmp(buffer, "folder in client exists") == 0) {
        folder_client_exist = TRUE;
    }
}

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

void receive_file(int socket_fd, const char *base_path) {
    char path_buffer[BUFFER_SIZE];
    char file_name[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    // Nhận đường dẫn thư mục từ client
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
    char file_dir[512];
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

//hàm gửi file
void send_file(int socket_fd, const char *path_file, const char *file_path) {
    char different_path[256];
    get_different_path(file_path, path_file, different_path);
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
    printf("different_path: %s\n", different_path);
    if (send(socket_fd, different_path, strlen(different_path) + 1, 0) == -1) {
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

int check_directory_exists(const char *parent_dir, const char *dir_name) {
    struct dirent *entry;
    struct stat entry_stat;
    char full_path[1024];

    // Mở thư mục cha
    DIR *dir = opendir(parent_dir);
    if (!dir) {
        perror("Không thể mở thư mục");
        return 0; // Thư mục không mở được
    }

    // Duyệt qua các mục trong thư mục
    while ((entry = readdir(dir)) != NULL) {
        // Bỏ qua "." và ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Tạo đường dẫn đầy đủ
        snprintf(full_path, sizeof(full_path), "%s/%s", parent_dir, entry->d_name);

        // Kiểm tra loại mục (file/thư mục)
        if (stat(full_path, &entry_stat) == 0 && S_ISDIR(entry_stat.st_mode)) {
            // Kiểm tra xem có trùng với tên thư mục cần tìm không
            if (strcmp(entry->d_name, dir_name) == 0) {
                closedir(dir);
                return 1; // Tìm thấy thư mục
            }
        }
    }

    // Đóng thư mục sau khi duyệt
    closedir(dir);
    return 0; // Không tìm thấy thư mục
}

int is_directory_exists(const char *path) {
    struct stat info;

    // Sử dụng stat để kiểm tra thông tin của đường dẫn
    if (stat(path, &info) != 0) {
        // Không thể lấy thông tin về đường dẫn
        return 0;
    }

    // Kiểm tra xem đường dẫn có phải là thư mục hay không
    return (info.st_mode & S_IFDIR) != 0;
}

void remove_last_component(char *path) {
    char *last_slash = strrchr(path, '/'); // Tìm dấu '/' cuối cùng
    if (last_slash != NULL) {
        *last_slash = '\0'; // Kết thúc chuỗi tại dấu '/'
    }
}

// Hàm lấy thư mục cuối cùng trong đường dẫn
void get_last_directory(const char *path, char *last_directory) {
    // Tạo bản sao của đường dẫn để tránh thay đổi đường dẫn gốc
    char path_copy[BUFFER_SIZE];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';  // Đảm bảo kết thúc chuỗi

    // Dùng strtok để tách đường dẫn tại dấu '/'
    char *token = strtok(path_copy, "/");

    // Lặp qua tất cả các phần của đường dẫn và lấy phần cuối cùng
    while (token != NULL) {
        strncpy(last_directory, token, BUFFER_SIZE - 1);
        last_directory[BUFFER_SIZE - 1] = '\0';  // Đảm bảo kết thúc chuỗi
        token = strtok(NULL, "/");
    }
}

// Hàm nhận đường dẫn từ client
int receive_directory_path(int client_socket, char *dir_path, size_t size) {
    // Đọc dữ liệu từ client
    int bytes_received = recv(client_socket, dir_path, size - 1, 0);
    if (bytes_received < 0) {
        perror("Lỗi khi nhận dữ liệu");
        return -1;  // Lỗi trong việc nhận dữ liệu
    }
    
    if (bytes_received == 0) {
        printf("Client đã đóng kết nối\n");
        return 0;  // Client đã đóng kết nối
    }

    // Đảm bảo kết thúc chuỗi đúng cách
    dir_path[bytes_received] = '\0';

    // Gửi phản hồi lại cho client
    const char *response = "File info received successfully.";
    send_response(client_socket, response);

    return 1;  // Thành công
}

// Hàm tạo socket
int create_server_socket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

// Hàm liên kết socket với địa chỉ và cổng
void bind_server_socket(int server_fd, struct sockaddr_in *server_addr) {
    if (bind(server_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
}

// Hàm lắng nghe kết nối từ client
void listen_for_connections(int server_fd) {
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d...\n", PORT);
}

// Hàm nhận thông tin file từ client và lưu vào file_info
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

int main() {
    //tạo thư mục lưu trữ
    create_storage();

    char first_directory[BUFFER_SIZE];
    char absolute_path[1024]; // Đường dẫn tuyệt đối
    char full_path[1024]; // Đường dẫn đầy đủ


    int server_fd, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    // Cấu hình địa chỉ server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Tạo socket
    server_fd = create_server_socket();

    // Liên kết socket với địa chỉ
    bind_server_socket(server_fd, &server_addr);

    // Lắng nghe kết nối từ client
    listen_for_connections(server_fd);

    while(1)
    {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Lỗi khi chấp nhận kết nối");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        printf("Client connected\n");

        //nhận câu lệnh từ client
        char command[10];
        int command_receive;
        command_receive = recv(client_socket, command, sizeof(command) - 1, 0);
        // Đảm bảo chuỗi kết thúc bằng NULL
        command[command_receive] = '\0';
        printf("Đã nhận command từ client: %s\n", command);
        const char *rp = "received command";
        send_response(client_socket, rp);

        if (strcmp(command, "rsync") == 0)
        {
            char dir_path[BUFFER_SIZE];
            int file_count = 0;
            //nhận đường dẫn thư mục ở client
            int result = receive_directory_path(client_socket, dir_path, sizeof(dir_path));
            if (result == 1) {
                printf("Đường dẫn nhận được từ client: %s\n", dir_path);
            } else {
                printf("Lỗi khi nhận đường dẫn từ client\n");
            }

            memset(dir_path, '\0', sizeof(dir_path));
            //nhận đường dẫn thư mục sẽ lưu trên server
            receive_directory_path(client_socket, dir_path, sizeof(dir_path));

            // Chuyển STORAGE_PATH thành đường dẫn tuyệt đối
            if (realpath(STORAGE_PATH, absolute_path) == NULL) {
                perror("Lỗi khi lấy đường dẫn tuyệt đối");
                return 1;
            }
            

            // Lấy thư mục đầu tiên
            //get_last_directory(dir_path, first_directory);

            // Nối đường dẫn tuyệt đối với đường dẫn nhận được
            snprintf(full_path, sizeof(full_path), "%s/%s", absolute_path, dir_path);

            printf("Đường dẫn thư mục sẽ lưu trên server: %s\n", full_path);

            // In thư mục đầu tiên
            printf("Thư mục sẽ lưu: %s\n", dir_path);

            //remove_last_component(full_path);

            if (check_path_exists(absolute_path, dir_path)) 
            {
                printf("Đường dẫn %s/%s tồn tại.\n", absolute_path, dir_path);
                
                const char *response_message = "folder exists";
                send_response(client_socket, response_message);
                FileInfo file_list[1000];  // Mảng lưu thông tin tệp tin duyệt trong server
                FileInfo file_info[1000];   //Mảng lưu thông tin tệp tin nhận từ client

                // Liệt kê các tệp tin và lưu thông tin
                snprintf(absolute_path + strlen(absolute_path), sizeof(absolute_path) - strlen(absolute_path), "/%s", dir_path);
                printf("absolute path: %s\n", absolute_path);
                list_files(absolute_path, file_list, &file_count);

                printf("\n\n");

                int file_count_from_client;
                receive_file_count(client_socket, &file_count_from_client);
                printf("file count receive from client: %d\n", file_count_from_client);
                const char *response = "File count received successfully.";
                send_response(client_socket, response);

                //Nhận thông tin file bên client gửi đến để so sánh
                for(int i = 0; i < file_count_from_client; i++)
                {
                    receive_file_info(client_socket, &file_info[i]);
                    const char *mess = "Receive infor file ok!";
                    send_response(client_socket, mess);
                    printf("\n");

                    //ghép đường dẫn
                    char full_path1[BUFFER_SIZE]; 
                    snprintf(full_path1, sizeof(full_path1), "%s%s%s", absolute_path, file_info[i].filepath, file_info[i].filename);
                    strncpy(file_info[i].filepath, full_path1, sizeof(file_info[i].filepath) - 1);
                    file_info[i].filepath[sizeof(file_info[i].filepath) - 1] = '\0';  // Đảm bảo chuỗi kết thúc đúng
                    remove_last_component(full_path1);

                    //In ra thông tin file nhận được
                    printf("ClientToServer_Tệp tin: %s\n", file_info[i].filename);
                    printf("ClientToServer_Đường dẫn: %s\n", file_info[i].filepath);
                    printf("ClientToServer_Kích thước: %ld bytes\n", file_info[i].filesize);
                    printf("ClientToServer_Hash (MD5): ");
                    for (int j = 0; j < 16; j++) {
                        printf("%02x", file_info[i].hash[j]);
                    }
                    printf("\n");

                    //kiểm tra xem file có tồn tại hay là không (folder có tồn tại nhưng file chưa chắc đã tồn tại)
                    if(!check_file_exists(file_info[i].filepath))
                    {
                        printf("%s không tồn tại!\n", file_info[i].filename);
                        const char *response = "File no exist";
                        send_response(client_socket, response);
                        //remove_last_component(full_path);
                        receive_file(client_socket, full_path);
                        const char *send_file_message = "File received successfully.";
                        send_response(client_socket, send_file_message);
                        // receive_file(client_socket, full_path);
                        // const char *response = "File received successfully.";
                        // send_response(client_socket, response);
                    }
                    else
                    {            
                        printf("%s tồn tại!\n", file_info[i].filename);
                        printf("\n");
                        const char *response = "File exist";
                        send_response(client_socket, response);

                        for(int index = 0; index < file_count; index++)
                        {
                            if (strcmp(file_list[index].filename, file_info[i].filename) == 0) {
                                // Hai tên file giống nhau
                                if (!(memcmp(file_list[index].hash, file_info[i].hash, sizeof(file_list[index].hash)) == 0))
                                {
                                    printf("File %s có thay đổi\n", file_list[index].filename);
                                    printf("Server_Hash Server (MD5): ");
                                    for (int j = 0; j < 16; j++) {
                                        printf("%02x", file_list[index].hash[j]);
                                    }
                                    printf("\n");
                                    printf("Server_Hash Client (MD5): ");
                                    for (int j = 0; j < 16; j++) {
                                        printf("%02x", file_info[i].hash[j]);
                                    }
                                    printf("\n");
                                    const char *response = "File change";
                                    send_response(client_socket, response);
                                    receive_file(client_socket, full_path);
                                    const char *send_file_message = "File received successfully.";
                                    send_response(client_socket, send_file_message);
                                    printf("File thay đổi là: %s\n", file_list[index].filename);
                                    break;
                                }

                                else
                                {
                                    printf("File %s không thay đổi\n", file_list[index].filename);
                                    const char *response = "File no change";
                                    send_response(client_socket, response);
                                    break;
                                }
                            }
                        }
                    }
                    printf("\n\n");
                }
            } 

            else {
                printf("Đường dẫn %s/%s không tồn tại.\n", absolute_path, dir_path);
                const char *response_message = "folder no exists";
                send_response(client_socket, response_message);  
                while(client_socket >= 0)
                {
                    receive_file(client_socket, full_path);
                    const char *response = "File received successfully.";
                    send_response(client_socket, response);
                }
            }
        }

        else if(strcmp(command, "ls") == 0)
        {
            printf("Hiên thị cây thư mục!\n");
            print_tree(STORAGE_PATH, 0, client_socket);
        }

        else if(strcmp(command, "clone") == 0)
        {
            FileInfo file_list[1000];
            int file_count = 0;
            char dir_path[BUFFER_SIZE];
            //nhận đường dẫn thư mục từ client
            int rs = receive_directory_path(client_socket, dir_path, sizeof(dir_path));
            if (rs == 1) {
                // Chuyển STORAGE_PATH thành đường dẫn tuyệt đối
                if (realpath(STORAGE_PATH, absolute_path) == NULL) {
                    perror("Lỗi khi lấy đường dẫn tuyệt đối");
                    return 1;
                }

                // In ra giá trị dir_path trước khi ghép
                printf("dir_path trước khi ghép: %s\n", dir_path);

                // Ghép đường dẫn tuyệt đối với dir_path

                char result_path[128];
                // Kiểm tra và ghép chuỗi
                if (dir_path[0] == '/') {
                    // Nếu dir_path đã là đường dẫn tuyệt đối
                    snprintf(result_path, sizeof(result_path), "%s%s", absolute_path, dir_path);
                } else {
                    // Nếu dir_path là đường dẫn tương đối
                    snprintf(result_path, sizeof(result_path), "%s/%s", absolute_path, dir_path);
                }
                // In ra đường dẫn kết quả
                printf("Đường dẫn nhận được từ client: %s\n", result_path);

                //kiểm tra xem folder có tồn tại hay không
                if (is_directory_exists(result_path)) {
                    printf("Thư mục '%s' tồn tại.\n", result_path);
                    const char *response_message = "folder exists";
                    send_response(client_socket, response_message);  
                    list_files(result_path, file_list, &file_count);

                    receive_response(client_socket);

                    //nhận phản hồi để kiểm tra xem folder lưu trên client có tồn tại hay không
                    //Nếu folder lưu trên client không tồn tại
                    if(folder_client_exist == FALSE)
                    {  
                        //Nếu không tồn tại thì gửi toàn bộ xuống thư mục xuống
                        // In ra thông tin của các tệp tin
                        for (int i = 0; i < file_count; i++) {
                            send_file(client_socket, file_list[i].filepath, result_path);
                            receive_response(client_socket);
                            printf("\n");
                        }
                        folder_client_exist = TRUE;
                    }

                    //Nếu folder lưu trên client tồn tại
                    else
                    {
                        //Đầu tiên phải gửi file count đến cho client
                        send_file_count(client_socket, file_count);
                        receive_response(client_socket);
                    }
                } 

                else {
                    printf("Thư mục '%s' không tồn tại.\n", result_path);
                    const char *response_message = "folder no exists";
                    send_response(client_socket, response_message);  
                }
            } 
            else 
            {
                printf("Lỗi khi nhận đường dẫn từ client\n");
            }

        }
    }

    // Đóng server socket
    close(server_fd);
    return 0;
}