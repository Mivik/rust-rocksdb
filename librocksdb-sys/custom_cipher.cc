
#include "rocksdb/env_encryption.h"

#include <fstream>

using ROCKSDB_NAMESPACE::BlockAccessCipherStream;
using ROCKSDB_NAMESPACE::EncryptionProvider;
using ROCKSDB_NAMESPACE::Env;
using ROCKSDB_NAMESPACE::EnvOptions;
using ROCKSDB_NAMESPACE::Slice;
using ROCKSDB_NAMESPACE::Status;

struct CustomCipher {
  size_t MetadataSize, BlockSize;
  bool (*EncryptBlock)(uint64_t blockIndex, char *data, char *metadata);
  bool (*DecryptBlock)(uint64_t blockIndex, char *data, char *metadata);
};

class CustomCipherStream : public BlockAccessCipherStream {
public:
  CustomCipherStream(const std::string &fname, CustomCipher cipher)
      : _file(fname + ".aead",
              std::fstream::in | std::fstream::out | std::fstream::binary),
        _cipher(cipher) {}

  size_t BlockSize() override { return _cipher.BlockSize; }

protected:
  void AllocateScratch(std::string &s) override {
    s.reserve(_cipher.MetadataSize);
  }

  Status EncryptBlock(uint64_t blockIndex, char *data, char *scratch) override {
    if (!_cipher.EncryptBlock(blockIndex, data, scratch)) {
      return Status::Corruption();
    }
    _file.seekg(blockIndex * _cipher.MetadataSize);
    _file.write(scratch, _cipher.MetadataSize);
    return Status::OK();
  }

  Status DecryptBlock(uint64_t blockIndex, char *data, char *scratch) override {
    _file.seekg(blockIndex * _cipher.MetadataSize);
    _file.read(scratch, _cipher.MetadataSize);
    if (!_cipher.DecryptBlock(blockIndex, data, scratch)) {
      return Status::Corruption();
    }
    return Status::OK();
  }

private:
  std::fstream _file;
  CustomCipher _cipher;
};

class CustomEncryptionProvider : public EncryptionProvider {
public:
  CustomEncryptionProvider(CustomCipher cipher) : _cipher(cipher) {}

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

void *rocksdb_create_encrypted_env(
    size_t metadata_size, size_t block_size,
    bool (*encrypt_block)(uint64_t block_index, char *data, char *metadata),
    bool (*decrypt_block)(uint64_t block_index, char *data, char *metadata)) {
  CustomCipher cipher = {metadata_size, block_size, encrypt_block,
                         decrypt_block};
  std::shared_ptr<EncryptionProvider> provider(
      new CustomEncryptionProvider(cipher));
  return NewEncryptedEnv(Env::Default(), provider);
}
