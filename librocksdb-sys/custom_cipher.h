
#pragma once

#ifdef _WIN32
#ifdef ROCKSDB_DLL
#ifdef ROCKSDB_LIBRARY_EXPORTS
#define ROCKSDB_LIBRARY_API __declspec(dllexport)
#else
#define ROCKSDB_LIBRARY_API __declspec(dllimport)
#endif
#else
#define ROCKSDB_LIBRARY_API
#endif
#else
#define ROCKSDB_LIBRARY_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct rocksdb_env_t rocksdb_env_t;

extern ROCKSDB_LIBRARY_API rocksdb_env_t *rocksdb_create_encrypted_env(
    void *userdata, size_t metadataSize, size_t blockSize,
    bool (*encryptBlock)(void *userdata, uint64_t blockIndex, char *data,
                          char *metadata),
    bool (*decryptBlock)(void *userdata, uint64_t blockIndex, char *data,
                          const char *metadata),
    void (*destroy)(void *userdata));

#ifdef __cplusplus
}
#endif
