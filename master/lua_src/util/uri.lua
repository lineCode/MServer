-- uri.lua
-- 2015-12-21
-- xzc

-- uri处理

--[[
https://www.ietf.org/rfc/rfc3986.txt

Reserved Characters:
reserved    = gen-delims / sub-delims
gen-delims  = ":" / "/" / "?" / "#" / "[" / "]" / "@"
sub-delims  = "!" / "$" / "&" / "'" / "(" / ")"
            / "*" / "+" / "," / ";" / "="

Unreserved Characters:
Characters that are allowed in a URI but do not have a reserved
purpose are called unreserved.  These include uppercase and lowercase
letters, decimal digits, hyphen, period, underscore, and tilde.

unreserved  = ALPHA / DIGIT / "-" / "." / "_" / "~"
]]

--[[
https://www.ietf.org/rfc/rfc2396.txt

      unreserved  = alphanum | mark

      mark        = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
]]

--[[
https://www.ietf.org/rfc/rfc1738.txt

alpha          = lowalpha | hialpha
digit          = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
                 "8" | "9"
safe           = "$" | "-" | "_" | "." | "+"
extra          = "!" | "*" | "'" | "(" | ")" | ","
national       = "{" | "}" | "|" | "\" | "^" | "~" | "[" | "]" | "`"
punctuation    = "<" | ">" | "#" | "%" | <">


reserved       = ";" | "/" | "?" | ":" | "@" | "&" | "="
hex            = digit | "A" | "B" | "C" | "D" | "E" | "F" |
                 "a" | "b" | "c" | "d" | "e" | "f"
escape         = "%" hex hex

unreserved     = alpha | digit | safe | extra
uchar          = unreserved | escape
xchar          = unreserved | reserved | escape
digits         = 1*digit
]]

local Uri = {}

function Uri.encode( str )
    -- rfc1738  rfc2396 *不被编码，~会被编码。这个版本在php、java中较为通用
    -- rfc3986  *会被编码，~不会被编码
    -- 空格为%20
    return string.gsub(str, "([^%w%.%-_~])", function(c) return string.format("%%%02X", string.byte(c)) end)

    -- 部分版本空格是+，请使用以下版本
    -- str = string.gsub(str, "([^%w%.%-_~ ])", function(c) return string.format("%%%02X", string.byte(c)) end)
    -- return string.gsub(str, " ", "+")
end

function Uri.decode( str )
    return string.gsub(str, '%%(%x%x)', function(h) return string.char(tonumber(h, 16)) end)
end

--[[
str = /platform/pay?sid=99&money=200
则返回 /platform/pay {sid = 99,money = 200}
]]
function Uri.parse( str )
    local indices = string.find( str, "?", 1, true );

    -- 没有问号，不带任何参数
    if not indices then return str end

    local url_str = string.sub( str,1,indices - 1 )
    local field_str = string.sub( str,indices )

    local fields = {}
    for k, v in string.gmatch(field_str, "([%w%.%-_~%%]*)=([%w%.%-_~%%]*)") do
        fields[k] = v
    end

    return url_str,fields
end

return Uri
