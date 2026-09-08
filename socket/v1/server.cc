#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <cstdio>
#include <cstddef>
#include <cerrno>


using std::cout;
using std::cin;
using std::endl;

// 保证把全部的数据发送出去的辅助函数。
// 适用于阻塞式 Linux TCP Socket
bool sendAll(int fd, const void* data, std::size_t length){
    const auto* bytes = static_cast<const char*>(data);

    std::size_t total_sent = 0;

    // 循环调用send()，直到全部字节发送完成。
    while(total_sent < length){
        const ssize_t sent = send(
            fd,
            bytes + total_sent,
            length - total_sent,
            MSG_NOSIGNAL
        );

        if(sent > 0){
            total_sent += static_cast<std::size_t>(sent);
            continue;
        }
        
        if(sent == -1 && errno == EINTR){
            // send() 被信号中断；数据不一定发送失败，重试即可。
            continue;
        }

        if(sent == 0){
             // 此时仍有数据未发送，若继续循环会造成死循环。
            errno = EPIPE;
        }
        return false;
    }
    return true;
}

int main(){

    // 创建socket文件描述符
    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd == -1){
        perror("socket faild!");
        return 1;
    }

    // 配置服务端地址
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本机所有网卡
    server_addr.sin_port = htons(2000); // 端口需转换成网络字节序

    // 绑定地址和端口
    if(bind(listen_fd, reinterpret_cast<const sockaddr*>(&server_addr), sizeof(server_addr)) == -1){
        perror("bind faild!");
        close(listen_fd);
        return 1;
    }

    // 监听连接
    if(listen(listen_fd, 10) == -1){
        perror("listen faild!");
        close(listen_fd);
        return 1;
    };

    cout << "server start, listening on 0.0.0.0:2000\n";

    while(1){
        sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        
        // 接受一个新的客户端连接
        const int client_fd = accept(
            listen_fd, 
            reinterpret_cast<sockaddr*>(&client_addr), 
            &len
        ); 
        if(client_fd == -1){
            if(errno == EINTR){
                // accept 被信号中断，监听socket仍可继续使用
                continue;
            }
            perror("accept faild!");
            break;;
        }

        cout << "A client connection was successful\n";
    
        while(1){
            char buffer[1024] = {0};

            const ssize_t received  = recv(
                client_fd, 
                buffer, 
                1024, 
                0
            );

            if(received > 0){
                cout << "Received " << received
                    << "byte(s) from client: ";
                // 按实际接收长度输出，不依赖 '\0' 结尾。
                cout.write(buffer, static_cast<std::streamsize>(received));
                cout << '\n';
            }else if(received == 0){
                // TCP 中 recv() 返回 0：对端正常关闭连接。
                cout << "Client disconnected\n";
                break;
            }else{
                // received == -1
                if (errno == EINTR) {
                    // recv 被信号中断；重试本次接收。
                    continue;
                }

                if (errno == ECONNRESET) {
                // 客户端异常退出、网络重置等。
                cout << "Client disconnected unexpectedly: connection reset\n";
                } else {
                    perror("recv");
                }

                break;
            }
            
            cout << "I want send message for client : " << std::flush;
            std::string line;
            if(!std::getline(std::cin, line)){
                break;
            }
            if(!sendAll(client_fd, line.data(), line.size())){
                perror("send faild!");
                break;
            }
            cout << "Server message sent successfully!\n";
        }
        close(client_fd);
        std::cout << "Client socket closed\n";
    }   
    close(listen_fd);
    return 0;
}