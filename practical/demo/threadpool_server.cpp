#include <iostream>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <queue>
#include <functional>

using Task = std::function<void()>;
std::queue<Task> task_queue;        // 任务队列
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;     // 保护队列的锁
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;       // 条件变量
bool running = true;        // 线程池开关

void* worker_thread(void* arg) {
    (void)arg;   // 未使用，pthread 签名要求
    while(true) {
        Task task;

        pthread_mutex_lock(&queue_lock);
        while(running && task_queue.empty()) {
            // 队列空---睡觉 等主线程扔任务进来
            pthread_cond_wait(&queue_cond, &queue_lock);
        }

        if(!running && task_queue.empty()) {
            pthread_mutex_unlock(&queue_lock);
            break;      // 程序要退出 队列也空了 -> 线程退出
        }

        task = task_queue.front();
        task_queue.pop();
        pthread_mutex_unlock(&queue_lock);

        task();     // 执行任务
    }
    return nullptr;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    std::cout << "[线程池聊天室] 监听 0.0.0.0:9999\n";

    std::vector<int> clients;

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);
    struct epoll_event events[1024];

    const int THREAD_COUNT = 4;
    for(int i = 0; i < THREAD_COUNT; ++i) {
        pthread_t tid;
        pthread_create(&tid, nullptr, worker_thread, nullptr);
        pthread_detach(tid);    // 线程结束后内核自动回收
    }

    while(true) {
        int n = epoll_wait(epoll_fd, events, 1024, -1);

        for(int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if(fd == server_fd) {
                int client_fd = accept(server_fd, nullptr, nullptr);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                clients.push_back(client_fd);
                std::cout << "[聊天室] 新用户 fd=" << client_fd << "\n";
            }
            else {
                // 收到客户端消息 -> 交给线程池处理
                int client_fd = fd;
                char* buf = new char[1024];     // 每个任务独立的缓冲区
                int n = recv(client_fd, buf, 1023, 0);

                if(n <= 0) {
                    delete[] buf;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                    close(client_fd);
                    pthread_mutex_lock(&queue_lock);
                    for(auto it = clients.begin(); it != clients.end(); ++it) {
                        if(*it == client_fd) {
                            clients.erase(it);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&queue_lock);
                    std::cout << "[聊天室] 离开 fd=" << client_fd << "\n";
                }
                else {
                    buf[n] = '\0';
                    std::string msg(buf, n);    // 拷出消息内容

                    // 提交任务到线程池
                    pthread_mutex_lock(&queue_lock);
                    task_queue.push([&clients, client_fd, msg]() {
                        // 广播给所有人(除了发送者自己)
                        pthread_mutex_lock(&queue_lock);
                        for(int other : clients) {
                            if(other != client_fd) {
                                send(other, msg.c_str(), msg.size(), 0);
                            }
                        }
                        pthread_mutex_unlock(&queue_lock);
                    });
                    pthread_cond_signal(&queue_cond);
                    pthread_mutex_unlock(&queue_lock);

                    delete[] buf;
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
