//server.cpp
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

//面试考过的，如何设置非阻塞socket
int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

int main( int argc, char* argv[] )
{
    //这里为什么还要手动指定ip地址呢？我想可能是因为一台服务器可能有多个网卡
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    //tcp 流
    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );
    ret = listen( listenfd, 5 );
    assert( ret != -1 );
    
    //重置address，udp要用
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );
    
    //udp 数据报
    int udpfd = socket( PF_INET, SOCK_DGRAM, 0 );
    assert( udpfd >= 0 );
    ret = bind( udpfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );
	
    //epoll
    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );
    assert( epollfd != -1 );
    addfd( epollfd, listenfd );
    addfd( epollfd, udpfd );

    while( 1 )
    {	
	//epoll的timeout和poll的timeout解释是一样的
	//成功时返回就绪的文件描述符的个数
	//将就绪事件从内核事件表拷贝到第二个参数指定的数组中
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( number < 0 )
        {
            printf( "epoll failure\n" );
            break;
        }
   
       //	
        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
       	    //tcp监听套接字
	    if ( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                addfd( epollfd, connfd );
            }
	    //udp
            else if ( sockfd == udpfd )
            {
                char buf[ UDP_BUFFER_SIZE ];
                memset( buf, '\0', UDP_BUFFER_SIZE );
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );

                ret = recvfrom( udpfd, buf, UDP_BUFFER_SIZE-1, 0, ( struct sockaddr* )&client_address, &client_addrlength );
                if( ret > 0 )
                {
		    //回射
                    sendto( udpfd, buf, UDP_BUFFER_SIZE-1, 0, ( struct sockaddr* )&client_address, client_addrlength );
                }
            }
	    //这里肯定是连接套接字
	    //连接套接字发生了可读事件
            else if ( events[i].events & EPOLLIN )
            {
                char buf[ TCP_BUFFER_SIZE ];
                while( 1 )
                {
                    memset( buf, '\0', TCP_BUFFER_SIZE );
                    ret = recv( sockfd, buf, TCP_BUFFER_SIZE-1, 0 );
		    //一直读，读到socket空为止
                    if( ret < 0 )
                    {
			//非阻塞返回-1，再试一次！
			//说白了就是客户端没有进一步发送数据
                        if( ( errno == EAGAIN ) || ( errno == EWOULDBLOCK ) )
                        {
                            break;
                        }
			//如果返回值是-1且errno不属于上述情况，那么肯定出错了
			//直接关闭socket
                        close( sockfd );
                        break;
                    }
		    //The value 0 may also be returned if the requested number of bytes
       		    //to receive from a stream socket was 0.
		    //man page这样说的，没看懂啥意思
                    else if( ret == 0 )
                    {
                        close( sockfd );
                    }
		    //回射
                    else
                    {
                        send( sockfd, buf, ret, 0 );
                    }
                }
            }
            else
            {
                printf( "something else happened \n" );
            }
        }
    }

    close( listenfd );
    return 0;
}

