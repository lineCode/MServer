-- playe.lua
-- 2017-04-03
-- xzc

-- 玩家对象

local g_network_mgr = g_network_mgr

local gateway_session = g_unique_id:srv_session( 
    "gateway",tonumber(Main.srvindex),tonumber(Main.srvid) )

-- local Bag = require "item.bag"

--[[
玩家子模块，以下功能会自动触发
1. 创建对象
2. 加载数据库:db_load
3. 数据初始化:db_init
4. 定时存库  :db_save
5. 定时器    :on_timer
]]

local sub_module = 
{
    -- { name = "bag",new = Bag },
}

local Player = oo.class( nil,... )

function Player:__init( pid )
    self.pid = pid

    -- 创建各个子模块，这些模块包含统一的db存库、加载、初始化规则
    for _,module in pairs( sub_module ) do
        self[module.name] = module.new( self )
    end
end

-- 发送数据包到客户端
function Player:send_pkt( cfg,pkt )
    local srv_conn = g_network_mgr:get_srv_conn( gateway_session )

    return srv_conn:send_clt_pkt( self.pid,cfg,pkt )
end

-- 开始从db加载各模块数据
function Player:module_db_load()
    self.db_step = 0
    for _,module in pairs( sub_module ) do
        local module = self[module.name]
        if module.db_load then
            module.db_load()
            self.db_step = self.db_step + 1
        end
    end
end

return Player