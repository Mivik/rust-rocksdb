
#include "custom_cipher.h"

#include "rocksdb/env/composite_env_wrapper.h"
#include "rocksdb/env_encryption.h"
#include "rocksdb/file_system.h"

#include <cstdio>

using ROCKSDB_NAMESPACE::BlockAccessCipherStream;
using ROCKSDB_NAMESPACE::CompositeEnvWrapper;
using ROCKSDB_NAMESPACE::EncryptionProvider;
using ROCKSDB_NAMESPACE::Env;
using ROCKSDB_NAMESPACE::EnvOptions;
using ROCKSDB_NAMESPACE::FileSystem;
using ROCKSDB_NAMESPACE::FileSystemWrapper;
using ROCKSDB_NAMESPACE::IODebugContext;
using ROCKSDB_NAMESPACE::IOOptions;
using ROCKSDB_NAMESPACE::IOStatus;
using ROCKSDB_NAMESPACE::NewCompositeEnv;
using ROCKSDB_NAMESPACE::Slice;
using ROCKSDB_NAMESPACE::Status;

struct CustomCipher {
  void *userdata;
  size_t MetadataSize, BlockSize;
  bool (*EncryptBlock)(void *userdata, uint64_t blockIndex, char *data,
                       char *metadata);
  bool (*DecryptBlock)(void *userdata, uint64_t blockIndex, char *data,
                       const char *metadata);
  void (*Destroy)(void *userdata);
};

class CustomCipherStream : public BlockAccessCipherStream {
public:
  CustomCipherStream(const std::string &fname, CustomCipher cipher)
      : _cipher(cipher) {
    std::string meta = fname + ".meta";
    _file = fopen(meta.data(), "r+");
    if (!_file) {
      _file = fopen(meta.data(), "w+");
    }
  }

  size_t BlockSize() override { return _cipher.BlockSize; }

  ~CustomCipherStream() override { fclose(_file); }

protected:
  void AllocateScratch(std::string &s) override {
    s.reserve(_cipher.MetadataSize);
  }

  Status EncryptBlock(uint64_t blockIndex, char *data, char *scratch) override {
    fseeko(_file, blockIndex * _cipher.MetadataSize, SEEK_SET);
    size_t read = fread(scratch, 1, _cipher.MetadataSize, _file);
    memset(scratch + read, 0, _cipher.MetadataSize - read);
    if (!_cipher.EncryptBlock(_cipher.userdata, blockIndex, data, scratch)) {
      return Status::Corruption();
    }
    fseeko(_file, blockIndex * _cipher.MetadataSize, SEEK_SET);
    fwrite(scratch, _cipher.MetadataSize, 1, _file);
    return Status::OK();
  }

  Status DecryptBlock(uint64_t blockIndex, char *data, char *scratch) override {
    fseeko(_file, blockIndex * _cipher.MetadataSize, SEEK_SET);
    size_t read = fread(scratch, 1, _cipher.MetadataSize, _file);
    memset(scratch + read, 0, _cipher.MetadataSize - read);
    if (!_cipher.DecryptBlock(_cipher.userdata, blockIndex, data, scratch)) {
      return Status::Corruption();
    }
    return Status::OK();
  }

private:
  FILE *_file;
  CustomCipher _cipher;
};

class CustomEncryptionProvider : public EncryptionProvider {
public:
  CustomEncryptionProvider(CustomCipher cipher) : _cipher(cipher) {}
  ~CustomEncryptionProvider() override { _cipher.Destroy(_cipher.userdata); }

  const char *Name() const override { return "CustomEncryptionProvider"; }

  size_t GetPrefixLength() const override { return 0; }

  Status CreateNewPrefix(const std::string &fname, char *prefix,
                         size_t prefixLength) const override {
    return Status::OK();
  }

  Status AddCipher(const std::string &descriptor, const char *cipher,
                   size_t len, bool for_write) override {
    return Status::OK();
  }

  Status CreateCipherStream(
      const std::string &fname, const EnvOptions &options, Slice &prefix,
      std::unique_ptr<BlockAccessCipherStream> *result) override {
    (*result) = std::unique_ptr<BlockAccessCipherStream>(
        new CustomCipherStream(fname, _cipher));
    return Status::OK();
  }

private:
  CustomCipher _cipher;
};

class CustomFileSystem : public FileSystemWrapper {
public:
  explicit CustomFileSystem(const std::shared_ptr<FileSystem> &base)
      : FileSystemWrapper(base) {}

  const char *Name() const override { return "CustomFileSystem"; }

  IOStatus RenameFile(const std::string &src, const std::string &dest,
                      const IOOptions &options, IODebugContext *dbg) override {
    rename((src + ".meta").data(), (dest + ".meta").data());
    return FileSystemWrapper::RenameFile(src, dest, options, dbg);
  }
};

extern "C" {
struct rocksdb_env_t {
  Env *rep;
  bool is_default;
};

rocksdb_env_t *rocksdb_create_encrypted_env(
    void *userdata, size_t metadataSize, size_t blockSize,
    bool (*encryptBlock)(void *userdata, uint64_t blockIndex, char *data,
                         char *metadata),
    bool (*decryptBlock)(void *userdata, uint64_t blockIndex, char *data,
                         const char *metadata),
    void (*Destroy)(void *userdata)) {
  CustomCipher cipher = {userdata, metadataSize, blockSize, encryptBlock,
                         decryptBlock};
  std::shared_ptr<EncryptionProvider> provider(
      new CustomEncryptionProvider(cipher));

  auto my_fs = std::make_shared<CustomFileSystem>(FileSystem::Default());
  std::shared_ptr<Env> my_env = NewCompositeEnv(my_fs);

  rocksdb_env_t *result = new rocksdb_env_t;
  result->rep =
      new CompositeEnvWrapper(my_env, NewEncryptedFS(my_fs, provider));
  result->is_default = true;
  return result;
}
}
