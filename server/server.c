#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>  

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

    // Chấp nhận kết nối từ client và xử lý
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Client accept failed");
            continue;
        }
        printf("Client connected\n");

        // Xử lý yêu cầu từ client
        handle_client(client_socket);
    }

    // Đóng server socket
    close(server_fd);
    return 0;
}
