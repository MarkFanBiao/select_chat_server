#include "./server_selectchat_nonblock.h"

int read_data(int sock)
{   //9999name|xx  或 9999msg|yy|nihao
#if 0
    char buflen[5];

    if(doRead(sock, buflen, 4) < 0)     //第一次没读到东西，说明socket出错了
    {                                   //doRead 有死循环，是阻塞的
        delUser(sock);
        return -1;
    }

    buflen[4] = 0;
    int len = atoi(buflen);

    char buf[8192];
    doRead(sock, buf, len);
    buf[len] = 0;
#endif
    //但是 若用read，只能读一次，不知道报文有没有读全，或者粘包，
    //所以对每个客户端设置一个buf缓冲区，记录报文总长，和已经被接受的长度
    //可以把这个buf放在用户链表的用户结点中

    //当要接受数据的时候，首先要得到这个buf缓冲区，所以得到 用户user
    User *user = findUserBySock(sock);

    if(user->already_read_len < 4)
    {   //报文长度不知道，但报头的长度是4   //服务器端往user->buf中读
        int ret = read(sock, user->buf+user->already_read_len, 
                       4 - user->already_read_len);
        if(ret > 0)
        {   //记录收到的数据
            user->already_read_len += ret;
            //报头内容收到后，就知道报文一共多长了
            if(user->already_read_len == 4) 
            {
                user->buf[4] = 0;
                user->packet_len = atoi(user->buf);
            }
            return 0;   
        }
        else if(ret == 0)   //没读到东西，对方关闭了socket
        {
            delUser(sock);
            return -1;      //那么让我方也关闭相应的socket
        }
        else    //ret < 0
        {   //有两种情况：
            //一种是因为缓冲区没有数据，ret 返回-1
            if(errno == EAGAIN)
            {
                return 0;
            }
            //另一种是 socket出了问题，所以返回了 -1
            delUser(sock);
            return -1;
        }
    }       //报头 接受到数据 < 4,也就是报文长度未知的情况 完毕
    else    //user->already_read_len >= 4
    {
        int ret = read(sock, user->buf + user->already_read_len,
                       user->packet_len - (user->already_read_len - 4));

        if(ret > 0)     //读到数据了，就是报文剩下的数据
        {   //记录读到的数据
            user->already_read_len += ret;
            if(user->already_read_len == user->packet_len + 4)  //两次读全了
            {
                user->buf[user->already_read_len] = 0;  //设置 \0

                user->already_read_len = 0;    
                //当报文可以处理时，已读长度要重置成0，准备下一次循环的使用
            }
            else    //没读全
            {
                return 0;   //等待下一次再次接收数据
            }
        }
        else if(ret == 0)   //对方关了
        {
            delUser(sock);
            return -1;
        }
        else    //ret < 0
        {   //也是两种情况
            if(errno == EAGAIN)     //socket没数据，不是出错
                return 0;

            delUser(sock);      //socket 出了问题
            return -1;
        }
    }

    //程序走到这儿，读取的报文读全了，用一个buf记录一下报文内容
    char *buf = user->buf + 4;

    //对报文处理，初级处理————拆包
    char *cmd = strtok(buf, "|");
    if(strcmp(cmd, "name") == 0)
    {
        char *name = strtok(NULL, "\0");
        set_name(sock, name);
    }
    else if(strcmp(cmd, "msg") == 0)
    {
        char *toname = strtok(NULL, "|");
        char *content = strtok(NULL, "\0");
        send_msg(sock, toname, content);
    }
}

int main(int argc, char *argv[])
{
    int fdmax;
    fd_set set_back;

    int server = startTcpServer(9988, "0.0.0.0", 10);
    //将server 转换成 nonblock  //用fcntl 修改文件描述符属性
    //一般高性能 的服务器都是 nonblock的
    set_nonblock(server);

    fdmax = server;
    FD_ZERO(&set_back);
    FD_SET(server, &set_back);  //将server放到 描述符集合中去

    //进入select循环
    while(1)
    {
        fd_set set;
        memcpy(&set, &set_back, sizeof(set));

        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ret = select(fdmax+1, &set, NULL, NULL, &tv);
        if(ret > 0)
        {
            int fd;
            int fmx = fdmax;
            for(fd = 0; fd <= fmx; ++fd)
            {
                if(FD_ISSET(fd, &set)) 
                {
                    if(fd == server)
                    {
                        while(1)
                        {
                            int sock = accept(server, NULL, NULL);
                            if(sock == -1)  //接受失败
                                break;
                            //若将sock改成nonblock，而doRead 是阻塞的，所以不能用doRead 了
                            set_nonblock(sock);

                            FD_SET(sock, &set_back);
                            if(sock > fdmax)
                                fdmax = sock;

                            //建立连接以后, 通过sock增加一个用户
                            new_user(sock);
                            printf("some one connected\n");
                        }
                    }
                    else     //sock  客户端
                    {    //读取并处理数据
                        //如果返回值不是0，那么要把socket清理掉
                        //这里调用read_data函数，前面设成非阻塞
                        if(read_data(fd) != 0)
                        {
                            FD_CLR(fd, &set_back);
                            if(fd == fdmax)
                                fdmax--;
                            close(fd);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
