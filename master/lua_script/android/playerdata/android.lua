-- android.lua
-- 2015-10-05
-- xzc

-- 机器人对象

local Timer   = require "Timer"
local Socket  = require "Socket"
local Android = oo.class( nil,... )

local words   = { "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o",
                "p","q","r","s","t","u","v","w","x","y","z","0","1","2","3","4",
                "5","6","7","8","9" }

local max_word = 1024*64
local word = {}
local sz = #words
for i = 1,max_word do
    local srand = math.random(sz)
    table.insert( word,words[srand] )
end

local example = table.concat( word )

-- 构造函数
function Android:__init( pid )
    self.pid = pid
    self.send = 0
    self.recv = 0
end

-- 连接服务器
function Android:born( ip,port )
    local conn = Socket()
    conn:set_self( self )
    conn:set_read( self.talk_msg )
    conn:set_connected( self.alive )
    conn:set_disconnected( self.die )

    conn:connect( ip,port,Socket.CLIENT )
    self.conn = conn
end

-- 连接成功
function Android:alive( result )
    if not result then
        ELOG( "android born fail:" .. self.pid )
        return
    end


    local timer = Timer()
    timer:set_self( self )
    timer:set_callback( self.talk )
    timer:start( 0,1 )

    self.timer = timer
end

-- 断开连接
function Android:die()
    self.timer:stop()
    self.conn:kill()

    self.timer = nil
    self.conn = nil
    PLOG( "android die " .. self.pid )
end

-- 收到消息
function Android:talk_msg( pkt )
    self.recv = self.recv + 1
    if self.last_msg ~= pkt then
        ELOG( "android msg error,%s\n%s",self.last_msg,pkt )
        self:die()
        return
    end

    self.last_msg = nil
end

-- 发送消息
function Android:talk()
    if self.send ~= self.recv then
        PLOG( "wait,expect %d,got %d",self.send,self.recv )
        return
    end
    local str = string.sub( example,-math.random(max_word) )
    self.last_msg = str

    self.send = self.send + 1
    self.conn:raw_send( str )
    print( "talk " .. self.pid )
end

return Android