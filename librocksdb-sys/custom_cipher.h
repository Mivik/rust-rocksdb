
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

extern ROCKSDB_LIBRARY_API void *rocksdb_create_encrypted_env(
    size_t metadata_size, size_t block_size,
    bool (*encrypt_block)(uint64_t block_index, char *data, char *metadata),
    bool (*decrypt_block)(uint64_t block_index, char *data,
                          const char *metadata));

#ifdef __cplusplus
}
#endif

#undef ROCKSDB_LIBRARY_API
