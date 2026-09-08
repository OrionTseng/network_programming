#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

using std::cout;
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

int main(int argc, char* argv[]){
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1){
        perror("socket faild!");
        return 1;
    }

    // 绑定地址
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(2000);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // 连接服务器
    if(connect(fd, reinterpret_cast<const sockaddr*>(&server), sizeof(server)) == -1){
        perror("connect faild!");
        close(fd);
        return 1;
    }

    while(1){
        // 发送数据
        cout << "I want send message for client : " << std::flush;
        std::string line;
        if(!std::getline(std::cin, line)){
            break;
        }
        if(!sendAll(fd, line.data(), line.size())){
            perror("send faild!");
            break;
        }
        cout << "Server message sent successfully!\n";

        // 接受数据
        char buffer[1024] = {};
        const ssize_t received = recv(
            fd, 
            buffer, 
            sizeof(buffer), 
            0
        );
        if(received > 0){
            cout << "Received " 
            << " byte(s) from server : ";
            
            cout.write(buffer,static_cast<std::streamsize>(received));
            cout << "\n";
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

            perror("recv");
            break;
        }
    }
    close(fd);
    return 0;
}