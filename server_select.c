#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <cctype>

#define MAX_BUFFER_SIZE 1024

void normalizeString(char *str) {
    // Xóa ký tự dấu cách ở đầu và cuối xâu
    while (*str == ' ') {
        strcpy(str, str + 1);
    }
    while (str[strlen(str) - 1] == ' ') {
        str[strlen(str) - 1] = '\0';
    }

    // Xóa ký tự dấu cách thừa giữa các từ
    char *src = str;
    char *dst = str;
    int prevSpace = 0;

    while (*src) {
        if (*src == ' ') {
            if (!prevSpace) {
                *dst++ = *src;
                prevSpace = 1;
            }
        } else {
            *dst++ = *src;
            prevSpace = 0;
        }
        src++;
    }
    *dst = '\0';

    // Viết hoa chữ cái đầu các từ và viết thường các ký tự còn lại
    int capitalize = 1;
    for (int i = 0; i < strlen(str); i++) {
        if (str[i] == ' ') {
            capitalize = 1;
        } else {
            if (capitalize) {
                str[i] = toupper(str[i]);
                capitalize = 0;
            } else {
                str[i] = tolower(str[i]);
            }
        }
    }
}

int main() {
    
    int serverSocket;
    struct sockaddr_in serverAddr;

    // Tạo socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Lỗi khi tạo socket");
        return 1;
    }

    // Thiết lập địa chỉ và cổng cho socket
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr =inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(9000);

    // Gán địa chỉ và cổng cho socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Lỗi khi gán địa chỉ và cổng cho socket");
        return 1;
    }

    // Lắng nghe kết nối từ client
    if (listen(serverSocket, 5) < 0) {
        perror("Lỗi khi lắng nghe kết nối");
        return 1;
    }

    printf("Server đang lắng nghe kết nối từ client...\n");

    fd_set readfds;
    int maxfd = serverSocket + 1;
    int numClients = 0;
    int clientSockets[FD_SETSIZE];
    char clientBuffer[MAX_BUFFER_SIZE];

    while (1) {
        // Xóa tập hợp file descriptor đã đăng ký trước đó
        FD_ZERO(&readfds);

        // Thêm socket server và các client socket vào tập hợp file descriptor
        FD_SET(serverSocket, &readfds);
        for (int i = 0; i < numClients; i++) {
            FD_SET(clientSockets[i], &readfds);
        }

        // Sử dụng select để kiểm tra xem có kết nối mới hoặc dữ liệu từ client đến hay không
        int activity = select(maxfd, &readfds, NULL, NULL, NULL);

        if (activity == -1) {
            perror("Lỗi khi sử dụng select");
            return 1;
        }

        // Kiểm tra socket server
        if (FD_ISSET(serverSocket, &readfds)) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);

            // Chấp nhận kết nối từ client
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrLen);
            if (clientSocket < 0) {
                perror("Lỗi khi chấp nhận kết nối");
                return 1;
            }

            // Thêm client socket vào mảng
            clientSockets[numClients++] = clientSocket;
            printf("Client đã kết nối. Hiện có %d clients đang kết nối.\n", numClients);

            // Gửi xâu chào kèm số lượng client đang kết nối cho client vừa kết nối
            char welcomeMsg[MAX_BUFFER_SIZE];
            snprintf(welcomeMsg, sizeof(welcomeMsg), "Xin chào. Hiện có %d clients đang kết nối.\n", numClients);
            send(clientSocket, welcomeMsg, strlen(welcomeMsg), 0);
        }

        // Kiểm tra các client socket
        for (int i = 0; i < numClients; i++) {
            int clientSocket = clientSockets[i];
            if (FD_ISSET(clientSocket, &readfds)) {
                memset(clientBuffer, 0, sizeof(clientBuffer));

                // Nhận dữ liệu từ client
                ssize_t numBytes = recv(clientSocket, clientBuffer, sizeof(clientBuffer) - 1, 0);
                if (numBytes <= 0) {
                    // Đóng kết nối nếu gặp lỗi hoặc client đã ngắt kết nối
                    close(clientSocket);

                    // Xóa client socket khỏi mảng
                    for (int j = i; j < numClients - 1; j++) {
                        clientSockets[j] = clientSockets[j + 1];
                    }
                    numClients--;

                    printf("Client đã ngắt kết nối. Hiện có %d clients đang kết nối.\n", numClients);
                    continue;
                }

                // Chuẩn hóa xâu ký tự
                normalizeString(clientBuffer);

                // Kiểm tra xem client có gửi "exit" hay không
                if (strcmp(clientBuffer, "exit") == 0) {
                    // Gửi xâu chào tạm biệt
                    char goodbyeMsg[] = "Tạm biệt.\n";
                    send(clientSocket, goodbyeMsg, strlen(goodbyeMsg), 0);

                    // Đóng kết nối
                    close(clientSocket);

                    // Xóa client socket khỏi mảng
                    for (int j = i; j < numClients - 1; j++) {
                        clientSockets[j] = clientSockets[j + 1];
                    }
                    numClients--;

                    printf("Client đã ngắt kết nối. Hiện có %d clients đang kết nối.\n", numClients);
                } else {
                    // Gửi kết quả chuẩn hóa xâu ký tự cho client
                    send(clientSocket, clientBuffer, strlen(clientBuffer), 0);
                }
            }
        }
    }

    // Đóng socket server
    close(serverSocket);

    return 0;
}