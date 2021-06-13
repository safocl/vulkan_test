#pragma once

#include <xcb/dri3.h>
#include <xcb/damage.h>
#include <xcb/composite.h>
#include <xcb/present.h>
#include <xcb/shm.h>
#include <xcb/shape.h>
#include <xcb/xfixes.h>
#include <xcb/xcb.h>

#include <memory>

namespace xcbwraper {
#define CHECK_QUERY_VERSION( EXTENSION )                                          \
    inline void EXTENSION##CheckQueryVersion(                                     \
    xcb_connection_t * connection, int needMajorVersion, int needMinorVersion ) { \
        auto EXTENSION##QueryCookie = xcb_##EXTENSION##_query_version(            \
        connection, needMajorVersion, needMinorVersion );                         \
                                                                                  \
        xcb_generic_error_t * err {};                                             \
        std::unique_ptr< xcb_##EXTENSION##_query_version_reply_t >                \
        EXTENSION##QueryReply { xcb_##EXTENSION##_query_version_reply(            \
        connection, EXTENSION##QueryCookie, &err ) };                             \
                                                                                  \
        if ( err ) {                                                              \
            throw std::runtime_error(                                             \
            "##EXTENSION query version request failed with code error : " +       \
            std::to_string( static_cast< std::uint32_t >( err->error_code ) ) );  \
        }                                                                         \
    }

CHECK_QUERY_VERSION( composite )
CHECK_QUERY_VERSION( damage )
CHECK_QUERY_VERSION( dri3 )
CHECK_QUERY_VERSION( present )
CHECK_QUERY_VERSION( xfixes )

inline void shapeCheckQueryVersion( xcb_connection_t * connection ) {
    auto shapeQueryCookie = xcb_shape_query_version( connection );

    xcb_generic_error_t *                              err {};
    std::unique_ptr< xcb_shape_query_version_reply_t > shapeQueryReply {
        xcb_shape_query_version_reply( connection, shapeQueryCookie, &err )
    };

    if ( err ) {
        throw std::runtime_error(
        "shape query version request failed with code error : " +
        std::to_string( static_cast< std::uint32_t >( err->error_code ) ) );
    }
}

inline void shmCheckQueryVersion( xcb_connection_t * connection ) {
    auto shmQueryCookie = xcb_shm_query_version( connection );

    xcb_generic_error_t *                            err {};
    std::unique_ptr< xcb_shm_query_version_reply_t > shmQueryReply {
        xcb_shm_query_version_reply( connection, shmQueryCookie, &err )
    };

    if ( err ) {
        throw std::runtime_error(
        "shm query version request failed with code error : " +
        std::to_string( static_cast< std::uint32_t >( err->error_code ) ) );
    }
}

#undef CHECK_QUERY_VERSION
}   // namespace xcbwraper
