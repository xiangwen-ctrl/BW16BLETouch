#pragma once
#include <stdint.h>

using T_CLIENT_ID = uint8_t;
struct T_FUN_CLIENT_CBS {
    void (*disconnect_cb)(uint8_t);
};
extern const T_FUN_CLIENT_CBS* registeredClient;
inline void client_init(uint8_t) { registeredClient = nullptr; }
inline bool client_register_spec_client_cb(T_CLIENT_ID* id, const T_FUN_CLIENT_CBS* cb) {
    *id = 0;
    registeredClient = cb;
    return true;
}
