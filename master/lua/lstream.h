#ifndef __LSTREAM_H__
#define __LSTREAM_H__

#include <lua.hpp>

class lstream
{
public: /* lua interface */
    int protocol_end();
    int protocol_begin();

    int tag_int8 ();
    int tag_int16();
    int tag_int32();
    int tag_int64();

    int tag_uint8 ();
    int tag_uint16();
    int tag_uint32();
    int tag_uint64();

    int tag_string();

    int tag_array_end();
    int tag_array_begin();

    int pack();
    int unpack();

    static int read_int8 ( lua_State *L );
    static int read_int16( lua_State *L );
    static int read_int32( lua_State *L );
    static int read_int64( lua_State *L );

    static int read_uint8 ( lua_State *L );
    static int read_uint16( lua_State *L );
    static int read_uint32( lua_State *L );
    static int read_uint64( lua_State *L );

    static int write_int8 ( lua_State *L );
    static int write_int16( lua_State *L );
    static int write_int32( lua_State *L );
    static int write_int64( lua_State *L );

    static int write_uint8 ( lua_State *L );
    static int write_uint16( lua_State *L );
    static int write_uint32( lua_State *L );
    static int write_uint64( lua_State *L );

    static int read_string( lua_State *L );
    static int write_string( lua_State *L );

    /* allocate a buffer and push it to lua stack as userdata */
    static int allocate( lua_State *L );
public: /* c++ interface */
    /* convert a lua table into binary stream buffer */
    int pack_buffer( int mod,int func,const char *buffer,unsigned int size );
    /* convert binary stream buffer into a lua table */
    int unpack_buffer( int mod,int func,const char *buffer,unsigned int size );
};

#endif /* __LSTREAM_H__ */
