#include "lmap.h"
#include "ltools.h"
#include "lastar.h"

int32 lastar::search( lua_State *L ) // 寻路
{
    class lmap** udata = (class lmap**)luaL_checkudata( L, 1, "Map" );

    int32 x    = luaL_checkinteger(L,2); // 起点坐标x
    int32 y    = luaL_checkinteger(L,3); // 起点坐标x
    int32 dx   = luaL_checkinteger(L,4); // 终点坐标x
    int32 dy   = luaL_checkinteger(L,5); // 终点坐标x

    // 路径放到一个table里，但是C++里不会创建，由脚本那边传入并缓存，防止频繁创建引发gc
    const static int32 tbl_stack = 6;
    lUAL_CHECKTABLE(L,tbl_stack);

    class grid_map *map = *udata;
    if ( !map ) return 0;

    if ( !a_star::search(map,x,y,dx,dy) ) return 0;

    const std::vector<uint16> &path = a_star::get_path();

    size_t path_sz = path.size();
    // 路径依次存各个点的x,y坐标，应该是成双的
    if ( 0 != path_sz%2 ) return 0;

    int32 tbl_idx = 1;
    // 原来的路径是反向的，这里还原
    for (int32 idx = path_sz - 1;idx > 0;idx -= 2 )
    {
        // x坐标
        lua_pushinteger(L,path[idx - 1]);
        lua_rawseti(L,tbl_stack,tbl_idx++);

        // y坐标
        lua_pushinteger(L,path[idx]);
        lua_rawseti(L,tbl_stack,tbl_idx++);
    }

    // 设置table的n值为路径坐标数
    lua_pushstring(L,"n");
    lua_pushinteger(L,path_sz );
    lua_rawset(L,tbl_stack);

    // 返回路径格子数
    lua_pushinteger(L,path_sz/2);
    return 1;
}
