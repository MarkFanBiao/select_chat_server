#include "./server_select_chat.h"

int read_data(int sock)
{   //9999name|xx  或 9999msg|yy|nihao
    char buflen[5];
    
    if(doRead(sock, buflen, 4) < 0)     //第一次没读到东西，说明socket出错了
    {                                   //doRead 是阻塞的
        delUser(sock);
        return -1;
    }

    buflen[4] = 0;
    int len = atoi(buflen);

    char buf[8192];
    doRead(sock, buf, len);
    buf[len] = 0;

    char *cmd = strtok(buf, "|");
    if(strcmp(cmd, "name") == 0)
    {
        char *name = strtok(NULL, "\0");
        printf("someone changed name %s\n", name);
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
                        int sock = accept(server, NULL, NULL);
                        FD_SET(sock, &set_back);
                        if(sock > fdmax)
                            fdmax = sock;

                        //建立连接以后, 通过sock增加一个用户
                        new_user(sock);
                        printf("some one connected\n");
                    }
                    else     //sock  客户端
                    {    //读取并处理数据
                        //如果返回值不是0，那么要把socket清理掉
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
