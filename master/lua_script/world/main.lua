-- 游戏世界服务器入口

Main = {}       -- store dynamic runtime info to global
Main.command,Main.srvname,Main.srvindex,Main.srvid = ...

Main.wait = 
{
    gateway = 1,
    db_conn = 1, -- 等待连接db
}

require "modules.module_pre_header"

Main.session = g_unique_id:srv_session(
    Main.srvname,tonumber(Main.srvindex),tonumber(Main.srvid) )

require "modules.module_header"

-- 信号处理
function sig_handler( signum )
    Main.shutdown()

    ev:exit()
end

-- 主程序关闭
function Main.shutdown()
    g_mongodb_mgr:stop() -- 关闭所有数据库链接
end

-- 数据库连接成功
function Main.db_connected()
    Main.one_wait_finish( "db_conn",1 )
end

-- 检查需要等待的服务器是否初始化完成
function Main.one_wait_finish( name,val )
    if not Main.wait[name] then return end

    Main.wait[name] = Main.wait[name] - val
    if Main.wait[name] <= 0 then Main.wait[name] = nil end

    if table.empty( Main.wait ) then Main.final_init() end
end

function Main.init()
    Main.starttime = ev:time()
    network_mgr:set_curr_session( Main.session )

    g_command_mgr:load_schema()

    if not g_network_mgr:srv_listen( g_setting.sip,g_setting.sport ) then
        ELOG( "world server listen fail,exit" )
        os.exit( 1 )
    end

    g_network_mgr:connect_srv( g_setting.servers )

    -- 连接数据库
    g_mongodb:start( g_setting.mongo_ip,
        g_setting.mongo_port,g_setting.mongo_user,
        g_setting.mongo_pwd,g_setting.mongo_db,Main.db_connected )
end

function Main.final_init()
    Main.ok = true
    PLOG( "world server(0x%.8X) start OK",Main.session )
end

local function main()
    ev:signal( 2  )
    ev:signal( 15 )

    Main.init()

    ev:backend()
end

xpcall( main, __G__TRACKBACK__ )
