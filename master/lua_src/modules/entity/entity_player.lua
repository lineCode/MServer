-- entity_player.lua
-- xzc
-- 2018-11-20

-- 玩家实体，用于在场景显示

local Entity_animal = require "modules.entity.entity_animal"
local Entity_player = oo.class( Entity_animal,... )

function Entity_player:__init(eid,pid)
    Entity_animal.__init(self,eid,ET.PLAYER)
    self.pid = pid -- 玩家唯一id
end

-- 同步来自gw的基础属性
function Entity_player:update_base_info( base )
    for k,v in pairs(base) do self[k] = v end
end

-- 同步来自gw的玩家战斗属性
function Entity_player:update_battle_abt( abt_list )
    local sys = self.abt_sys:get_sys( ABTSYS.SYNC )

    sys:clear()
    for idx = 1,#abt_list,2 do
        sys:push_one( abt_list[idx],abt_list[idx+1] )
    end

    self.abt_sys:modify(true)
end

-- 创建实体出现的数据包
local tmp_pkt = {}
local tmp_player_pkt = {}
function Entity_player:appear_pkt()
    -- 创建基础数据
    Entity.appear_pkt(self,tmp_pkt)

    -- 创建玩家特有的数据
    tmp_player_pkt.pid = self.pid

    tmp_pkt.player = tmp_player_pkt

    return tmp_pkt
end

return Entity_player
