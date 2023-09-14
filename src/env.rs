use std::sync::Arc;

use libc::{self, c_int};

use crate::{ffi, Error};

/// An Env is an interface used by the rocksdb implementation to access
/// operating system functionality like the filesystem etc. Callers
/// may wish to provide a custom Env object when opening a database to
/// get fine gain control; e.g., to rate limit file system operations.
///
/// All Env implementations are safe for concurrent access from
/// multiple threads without any external synchronization.
///
/// Note: currently, C API behinds C++ API for various settings.
/// See also: `rocksdb/include/env.h`
#[derive(Clone)]
pub struct Env(pub(crate) Arc<EnvWrapper>);

pub(crate) struct EnvWrapper {
    pub(crate) inner: *mut ffi::rocksdb_env_t,
}

impl Drop for EnvWrapper {
    fn drop(&mut self) {
        unsafe {
            ffi::rocksdb_env_destroy(self.inner);
        }
    }
}

pub trait CustomCipher {
    fn encrypt_block(&self, block_index: u64, data: &mut [u8], metadata: &mut [u8]) -> bool;

    fn decrypt_block(&self, block_index: u64, data: &mut [u8], metadata: &[u8]) -> bool;
}

impl Env {
    /// Returns default env
    pub fn new() -> Result<Self, Error> {
        let env = unsafe { ffi::rocksdb_create_default_env() };
        if env.is_null() {
            Err(Error::new("Could not create mem env".to_owned()))
        } else {
            Ok(Self(Arc::new(EnvWrapper { inner: env })))
        }
    }

    /// Returns a new environment that stores its data in memory and delegates
    /// all non-file-storage tasks to base_env.
    pub fn mem_env() -> Result<Self, Error> {
        let env = unsafe { ffi::rocksdb_create_mem_env() };
        if env.is_null() {
            Err(Error::new("Could not create mem env".to_owned()))
        } else {
            Ok(Self(Arc::new(EnvWrapper { inner: env })))
        }
    }

    /// Creates a encrypted environment using custom cipher.
    pub fn encrypted(
        cipher: Box<dyn CustomCipher + Send + Sync>,
        metadata_size: usize,
        block_size: usize,
    ) -> Result<Self, Error> {
        struct CipherWrapper {
            inner: Box<dyn CustomCipher + Send + Sync>,
            metadata_size: usize,
            block_size: usize,
        }
        unsafe extern "C" fn encrypt_block(
            userdata: *mut libc::c_void,
            block_index: u64,
            data: *mut libc::c_char,
            metadata: *mut libc::c_char,
        ) -> bool {
            let cipher = &*(userdata as *const CipherWrapper);
            let data = std::slice::from_raw_parts_mut(data as *mut u8, cipher.block_size);
            let metadata =
                std::slice::from_raw_parts_mut(metadata as *mut u8, cipher.metadata_size);
            cipher.inner.encrypt_block(block_index, data, metadata)
        }
        unsafe extern "C" fn decrypt_block(
            userdata: *mut libc::c_void,
            block_index: u64,
            data: *mut libc::c_char,
            metadata: *const libc::c_char,
        ) -> bool {
            let cipher = &*(userdata as *const CipherWrapper);
            let data = std::slice::from_raw_parts_mut(data as *mut u8, cipher.block_size);
            let metadata = std::slice::from_raw_parts(metadata as *const u8, cipher.metadata_size);
            cipher.inner.decrypt_block(block_index, data, metadata)
        }
        unsafe extern "C" fn destroy(userdata: *mut libc::c_void) {
            let cipher = userdata as *mut CipherWrapper;
            cipher.drop_in_place();
        }

        let userdata = Box::leak(Box::new(CipherWrapper {
            inner: cipher,
            metadata_size,
            block_size,
        }));
        let env = unsafe {
            ffi::rocksdb_create_encrypted_env(
                userdata as *mut _ as _,
                metadata_size,
                block_size,
                Some(encrypt_block),
                Some(decrypt_block),
                Some(destroy),
            )
        } as *mut ffi::rocksdb_env_t;
        if env.is_null() {
            Err(Error::new("Could not create encrypted env".to_owned()))
        } else {
            Ok(Self(Arc::new(EnvWrapper { inner: env })))
        }
    }

    /// Sets the number of background worker threads of a specific thread pool for this environment.
    /// `LOW` is the default pool.
    ///
    /// Default: 1
    pub fn set_background_threads(&mut self, num_threads: c_int) {
        unsafe {
            ffi::rocksdb_env_set_background_threads(self.0.inner, num_threads);
        }
    }

    /// Sets the size of the high priority thread pool that can be used to
    /// prevent compactions from stalling memtable flushes.
    pub fn set_high_priority_background_threads(&mut self, n: c_int) {
        unsafe {
            ffi::rocksdb_env_set_high_priority_background_threads(self.0.inner, n);
        }
    }

    /// Sets the size of the low priority thread pool that can be used to
    /// prevent compactions from stalling memtable flushes.
    pub fn set_low_priority_background_threads(&mut self, n: c_int) {
        unsafe {
            ffi::rocksdb_env_set_low_priority_background_threads(self.0.inner, n);
        }
    }

    /// Sets the size of the bottom priority thread pool that can be used to
    /// prevent compactions from stalling memtable flushes.
    pub fn set_bottom_priority_background_threads(&mut self, n: c_int) {
        unsafe {
            ffi::rocksdb_env_set_bottom_priority_background_threads(self.0.inner, n);
        }
    }

    /// Wait for all threads started by StartThread to terminate.
    pub fn join_all_threads(&mut self) {
        unsafe {
            ffi::rocksdb_env_join_all_threads(self.0.inner);
        }
    }

    /// Lowering IO priority for threads from the specified pool.
    pub fn lower_thread_pool_io_priority(&mut self) {
        unsafe {
            ffi::rocksdb_env_lower_thread_pool_io_priority(self.0.inner);
        }
    }

    /// Lowering IO priority for high priority thread pool.
    pub fn lower_high_priority_thread_pool_io_priority(&mut self) {
        unsafe {
            ffi::rocksdb_env_lower_high_priority_thread_pool_io_priority(self.0.inner);
        }
    }

    /// Lowering CPU priority for threads from the specified pool.
    pub fn lower_thread_pool_cpu_priority(&mut self) {
        unsafe {
            ffi::rocksdb_env_lower_thread_pool_cpu_priority(self.0.inner);
        }
    }

    /// Lowering CPU priority for high priority thread pool.
    pub fn lower_high_priority_thread_pool_cpu_priority(&mut self) {
        unsafe {
            ffi::rocksdb_env_lower_high_priority_thread_pool_cpu_priority(self.0.inner);
        }
    }
}

unsafe impl Send for EnvWrapper {}
unsafe impl Sync for EnvWrapper {}
