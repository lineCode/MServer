#include "lsocket.h"
#include "ltools.h"
#include "leventloop.h"

lsocket::lsocket( lua_State *L )
    : L(L)
{
    ref_self       = 0;
    ref_message    = 0;
    ref_acception  = 0;
    ref_connection = 0;
    ref_disconnect = 0;
}

lsocket::~lsocket()
{
    socket::close(); /* lsocket的内存由lua控制，保证在释放socket时一定会关闭 */

    /* 释放引用，如果有内存问题，可查一下这个地方 */
    LUA_UNREF( ref_self       );
    LUA_UNREF( ref_message    );
    LUA_UNREF( ref_acception  );
    LUA_UNREF( ref_connection );
    LUA_UNREF( ref_disconnect );

    assert( "socket not clean",0 == sending && (!w.is_active()) );
}

int32 lsocket::kill()
{
    if ( !w.is_active() ) return 0;

    if ( _send.data_size() > 0 ) /* 尝试把缓冲区的数据直接发送 */
    {
        _send.send( w.fd );
    }

    socket::close();

    return 0;
}

int32 lsocket::listen()
{
    if ( w.is_active() )
    {
        return luaL_error( L,"listen:socket already active");
    }

    const char *addr = luaL_checkstring( L,1 );
    if ( !addr )
    {
        return luaL_error( L,"listen:address not specify" );
    }

    int32 port = luaL_checkinteger( L,2 );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        lua_pushnil( L );
        return 1;
    }

    int32 optval = 1;
    /*
     * enable address reuse.it will help when the socket is in TIME_WAIT status.
     * for example:
     *     server crash down and the socket is still in TIME_WAIT status.if try
     * to restart server immediately,you need to reuse address.but note you may
     * receive the old data from last time.
     */
    if ( setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,(char *) &optval, sizeof(optval)) < 0 )
    {
        ::close( fd );
        lua_pushnil( L );
        return 1;
    }

    if ( socket::non_block( fd ) < 0 )
    {
        ::close( fd );
        lua_pushnil( L );
        return 1;
    }

    struct sockaddr_in sk_socket;
    memset( &sk_socket,0,sizeof(sk_socket) );
    sk_socket.sin_family = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(addr);
    sk_socket.sin_port = htons( port );

    if ( ::bind( fd, (struct sockaddr *) & sk_socket,sizeof(sk_socket)) < 0 )
    {
        ::close( fd );

        lua_pushnil( L );
        return 1;
    }

    if ( ::listen( fd, 256 ) < 0 )
    {
        ::close( fd );
        lua_pushnil( L );
        return 1;
    }

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    w.set( loop );
    w.set<lsocket,&lsocket::listen_cb>( this );
    w.start( fd,EV_READ );

    lua_pushinteger( L,fd );
    return 1;
}

int32 lsocket::connect()
{
    if ( w.is_active() )
    {
        return luaL_error( L,"connect:socket already active");
    }

    const char *addr = luaL_checkstring( L,1 );
    if ( !addr )
    {
        return luaL_error( L,"connect:address not specify" );
    }

    const int32 port = luaL_checkinteger( L,2 );

    int32 fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if ( fd < 0 )
    {
        lua_pushnil( L );
        return 1;
    }

    if ( socket::non_block( fd ) < 0 )
    {
        lua_pushnil( L );
        return 1;
    }

    struct sockaddr_in sk_socket;
    memset( &sk_socket,0,sizeof(sk_socket) );
    sk_socket.sin_family = AF_INET;
    sk_socket.sin_addr.s_addr = inet_addr(addr);
    sk_socket.sin_port = htons( port );

    /* 三次握手是需要一些时间的，内核中对connect的超时限制是75秒 */
    if ( ::connect( fd, (struct sockaddr *) & sk_socket,sizeof(sk_socket)) < 0
        && errno != EINPROGRESS )
    {
        ::close( fd );

        lua_pushnil( L );
        return 1;
    }

    class ev_loop *loop = static_cast<class ev_loop *>( leventloop::instance() );
    w.set( loop );
    w.set<lsocket,&lsocket::connect_cb>( this );
    w.start( fd,EV_WRITE ); /* write事件 */

    lua_pushinteger( L,fd );
    return 1;
}

/* 发送原始数据，二进制或字符串 */
int32 lsocket::send()
{
    if ( !w.is_active() )
    {
        return luaL_error( L,"raw_send:socket not valid");
    }

    /* 不要用luaL_checklstring，它给出的长度不包含字符串结束符\0，而我们不知道lua发送的
     * 是字符串还是二进制，因此在lua层要传入一个长度，默认为字符串，由optional取值
     */
    const char *sz = luaL_checkstring( L,1 );
    const int32 len = luaL_optinteger( L,2,strlen(sz)+1 );

    if ( !sz || len <= 0 )
    {
        return luaL_error( L,"raw_send nothing to send" );
    }

    assert( "raw_send illegal fd",w.fd > 0 );

    /* 原始的发送函数,数据不经过协议处理(不经protobuf
     * 处理，不包含协议号...)，可以发送二进制或字符串。
     * 比如把战报写到文件，读出来后可以直接用此函数发送
     * 又或者向php发送json字符串
     */
    this->_send.append( sz,len );
    leventloop::instance()->pending_send( static_cast<class socket*>(this) );  /* 放到发送队列，最后一次发送 */

    return 0;
}

void lsocket::message_cb( ev_io &w,int32 revents )
{
    assert( "libev read cb error",!(EV_ERROR & revents) );

    int32 fd = w.fd;

    /* 就游戏中的绝大多数消息而言，一次recv就能接收完成，不需要while接收直到出错。而且
     * 当前设定的缓冲区与socket一致(8192)，socket缓冲区几乎不可能还有数据，不需要多调用
     * 一次recv。退一步，假如还有数据，epoll当前为LT模式，下一次回调再次读取
     * 如果启用while,需要检测_socket在lua层逻辑中是否被关闭
     */

    int32 ret = _recv.recv( fd );

    /* disconnect or error */
    if ( 0 == ret )
    {
        on_disconnect();
        return;
    }
    else if ( ret < 0 )
    {
        if ( EAGAIN != errno && EWOULDBLOCK != errno )
        {
            ERROR( "socket message_cb error:%s\n",strerror(errno) );
            on_disconnect();
        }
        return;
    }

    /* 此框架中，socket的内存由lua管理，无法预知lua会何时释放内存
     * 因此不要在C++层用while去解析协议
     */
    if ( !is_message_complete() ) return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_message);
    int32 param = 0;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "message_notify fail:%s\n",lua_tostring(L,-1) );
        return;
    }
}

void lsocket::on_disconnect()
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_disconnect);
    int32 param = 0;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "socket disconect,call lua fail:%s\n",lua_tostring(L,-1) );
        // DO NOT RETURN
    }

    /* 关闭fd，但不要delete
     * 先回调lua，再close.lua可能会调用一些函数，如取fd
     */
    socket::close();
}

int32 lsocket::address()
{
    if ( !w.is_active() )
    {
        return luaL_error( L,"listen:socket already active");
    }

    struct sockaddr_in addr;
    memset( &addr, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);

    if ( getpeername( w.fd, (struct sockaddr *)&addr, &len) < 0 )
    {
        return luaL_error( L,"getpeername error: %s",strerror(errno) );
    }

    lua_pushstring( L,inet_ntoa(addr.sin_addr) );
    return 1;
}

int32 lsocket::set_self_ref()
{
    if ( !lua_istable( L,1 ) )
    {
        return luaL_error( L,"set_self_ref,argument illegal.expect table" );
    }

    LUA_REF( ref_self );

    return 0;
}

int32 lsocket::set_on_message()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_message,argument illegal.expect function" );
    }

    LUA_REF( ref_message );

    return 0;
}

int32 lsocket::set_on_acception()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_acception,argument illegal.expect function" );
    }

    LUA_REF( ref_acception );

    return 0;
}

int32 lsocket::set_on_connection()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_connection,argument illegal.expect function" );
    }

    LUA_REF( ref_connection );

    return 0;
}

int32 lsocket::set_on_disconnect()
{
    if ( !lua_isfunction( L,1 ) )
    {
        return luaL_error( L,"set_on_disconnect,argument illegal.expect function" );
    }

    LUA_REF( ref_disconnect );

    return 0;
}

int32 lsocket::file_description()
{
    lua_pushinteger( L,w.fd );
    return 1;
}

/*
 * connect回调
 * man connect
 * It is possible to select(2) or poll(2) for completion by selecting the socket
 * for writing.  After select(2) indicates  writability,  use getsockopt(2)  to
 * read the SO_ERROR option at level SOL_SOCKET to determine whether connect()
 * completed successfully (SO_ERROR is zero) or unsuccessfully (SO_ERROR is one
 * of  the  usual  error  codes  listed  here,explaining the reason for the failure)
 * 1）连接成功建立时，socket 描述字变为可写。（连接建立时，写缓冲区空闲，所以可写）
 * 2）连接建立失败时，socket 描述字既可读又可写。 （由于有未决的错误，从而可读又可写）
 */
void lsocket::connect_cb ( ev_io &w,int32 revents )
{
    assert( "libev connect cb error",!(EV_ERROR & revents) );

    int32 fd = w.fd;

    int32 error   = 0;
    socklen_t len = sizeof (error);
    if ( getsockopt( fd, SOL_SOCKET, SO_ERROR, &error, &len ) < 0 )
    {
        ERROR( "connect cb getsockopt error:%s\n",strerror(errno) );

        error = errno;
        // DON NOT return
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref_connection);
    int32 param = 1;
    if ( ref_self )
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref_self);
        param ++;
    }
    lua_pushboolean( L,!error );
    if ( expect_false( LUA_OK != lua_pcall(L,param,0,0) ) )
    {
        ERROR( "connect_cb call lua fail:%s\n",lua_tostring(L,-1) );
        // DON NOT return
    }

    if ( error )  /* 连接失败 */
    {
        socket::close();
        ERROR( "connect unsuccess:%s\n",strerror(error) ); // not errno
        return;
    }

    KEEP_ALIVE( fd );
    USER_TIMEOUT( fd );

    w.stop();   /* 先stop再设置消耗要小一些 */
    w.set<lsocket,&lsocket::message_cb>( this );
    w.set( EV_READ ); /* 将之前的write改为read */
    w.start();
}