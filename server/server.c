#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>  
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define STORAGE_PATH "../common/storage"

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

// Hàm gửi response từ server về client
void send_response(int client_socket, const char *message) {
    if (send(client_socket, message, strlen(message), 0) < 0) {
        perror("Failed to send response");
    } else {
        printf("Response sent to client: %s\n", message);
    }
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
    if(path_buffer[0] == '\'')
    {
        snprintf(full_file_path, sizeof(full_file_path), "%s", base_path);
    }
    else{
        snprintf(full_file_path, sizeof(full_file_path), "%s/%s", base_path, path_buffer);
    }
    printf("đường dẫn thư mục tạo để ghi: %s\n", full_file_path);

    if(mkdir(full_file_path, 0777) == -1 && errno != EEXIST) {
        perror("Lỗi tạo thư mục");
        return;
    }
    char full_file_path2[BUFFER_SIZE];
    snprintf(full_file_path2, sizeof(full_file_path2), "%s/%s", full_file_path, file_name);

    printf("file name: %s\n", file_name);
    printf("đường dẫn file để ghi là: %s\n", full_file_path2);


    // Mở file để ghi dữ liệu nhận được
    int bytes_received;
    FILE *file = fopen(full_file_path2, "wb");
    if (!file) {
        perror("Lỗi mở file để ghi");
        return;
    }
    printf("Receiving file: %s\n", file_name);

    char buffer2[BUFFER_SIZE];
    while ((bytes_received = recv(socket_fd, buffer2, sizeof(buffer2), 0)) > 0) {
        fwrite(buffer2, 1, bytes_received, file);
        if (bytes_received < BUFFER_SIZE) break; // File truyền xong
    }

    fclose(file);
    printf("File '%s' received successfully!\n", file_name);
}



// Gửi phản hồi lại cho client
// void response_client(int client_socket)
// {
//     const char *response = "File info received successfully.";
//     send(client_socket, response, strlen(response), 0);
// }

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

// Hàm nhận thông tin tệp tin từ client
int receive_file_info(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Nhận thông tin tệp tin
    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        return -1; // Lỗi hoặc kết thúc kết nối
    }
    buffer[bytes_received] = '\0'; // Kết thúc chuỗi

    if (strcmp(buffer, "EOF") == 0) {
        return 0; // Tín hiệu kết thúc
    }

    // Hiển thị thông tin nhận được
    printf("Received file info:\n%s\n", buffer);
    return 1; // Thông tin tệp tin hợp lệ
}

// Hàm xử lý kết nối từ client
void handle_client(int client_socket) {
    while (1) {
        int status = receive_file_info(client_socket);
        if (status == 0) {
            printf("Client sent EOF. Closing connection.\n");
            break; // Thoát khi nhận EOF
        } else if (status == -1) {
            printf("Error receiving file info or client disconnected.\n");
            break; // Thoát khi xảy ra lỗi
        }

        // Gửi phản hồi lại cho client
        const char *response = "File info received successfully.";
        send(client_socket, response, strlen(response), 0);
    }

    // Đóng kết nối với client
    close(client_socket);
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

    client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket < 0) {
        perror("Lỗi khi chấp nhận kết nối");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client connected\n");
    char dir_path[BUFFER_SIZE];

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

    remove_last_component(full_path);

    if (check_path_exists(absolute_path, dir_path)) {
        printf("Đường dẫn %s/%s tồn tại.\n", absolute_path, dir_path);
        const char *response_message = "folder exists";
        send_response(client_socket, response_message);
    } else {
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

    // Nếu thư mục để lưu dữ liệu cần đồng bộ trên server chưa tồn tại thì tạo mới
    // if(!check_path_exists(absolute_path, dir_path))
    // {
    //     //full_path
        
    // }

    // Chấp nhận kết nối từ client và xử lý
    // while (client_socket >= 0) {
    //     // Xử lý yêu cầu từ client
    //     handle_client(client_socket);
    // }
    // perror("Client accept failed");

    // Đóng server socket
    close(server_fd);
    return 0;
}
