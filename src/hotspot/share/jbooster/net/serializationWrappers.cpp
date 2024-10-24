/*
 * Copyright (c) 2020, 2023, Huawei Technologies Co., Ltd. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include "jbooster/jBoosterManager.hpp"
#include "jbooster/net/communicationStream.inline.hpp"
#include "jbooster/net/serializationWrappers.inline.hpp"
#include "jbooster/utilities/fileUtils.hpp"
#include "runtime/os.inline.hpp"
#include "utilities/stringUtils.hpp"

// ------------------------- MemoryWrapper for Serializer --------------------------

MemoryWrapper::MemoryWrapper(const void* base, uint32_t size):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(base),
        _size(base == nullptr ? MessageConst::NULL_PTR : size),
        _allocater(nullptr) {
}

MemoryWrapper::MemoryWrapper(MemoryAllocater* allocater):
        WrapperBase(SerializationMode::DESERIALIZE),
        _base(nullptr),
        _size(MessageConst::NULL_PTR),
        _allocater(allocater) {}

MemoryWrapper::~MemoryWrapper() {
  if (!_free_memory || _base == nullptr) return;
  FREE_C_HEAP_ARRAY(char*, _base);
}

int MemoryWrapper::serialize(MessageBuffer& buf) const {
  _smode.assert_can_serialize();
  JB_RETURN(buf.serialize_no_meta(_size));
  if (_base == nullptr) {
    assert(_size == MessageConst::NULL_PTR, "sanity");
    return 0;
  }
  return buf.serialize_memcpy(_base, _size);
}

int MemoryWrapper::deserialize(MessageBuffer& buf) {
  _smode.assert_can_deserialize();
  assert(_base == nullptr, "can only be deserialized once");
  JB_RETURN(buf.deserialize_ref_no_meta(_size));
  if (_size == MessageConst::NULL_PTR) {
    return 0;
  }
  void* mem;
  if (_allocater != nullptr) {
    mem = _allocater->alloc(_size);
  } else {
    _free_memory = true;
    mem = (void*) NEW_C_HEAP_ARRAY(char, _size, mtJBooster);
  }
  buf.deserialize_memcpy(mem, _size);
  _base = mem;
  return 0;
}

// ------------------------- StringWrapper for Serializer --------------------------

StringWrapper::StringWrapper(const char* str):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(str),
        _size(str == nullptr ? (uint32_t) MessageConst::NULL_PTR : (uint32_t) strlen(str)) {}

StringWrapper::StringWrapper(const char* str, uint32_t size):
        WrapperBase(SerializationMode::SERIALIZE),
        _base(str),
        _size(str == nullptr ? (uint32_t) MessageConst::NULL_PTR : size) {
#ifdef ASSERT
  if (str != nullptr) {
    assert(strlen(str) >= size, "sanity");
  }
#endif
}

StringWrapper::StringWrapper():
        WrapperBase(SerializationMode::DESERIALIZE),
        _base(nullptr),
        _size(MessageConst::NULL_PTR) {}

StringWrapper::~StringWrapper() {
  if (!_free_memory || _base == nullptr) return;
  FREE_C_HEAP_ARRAY(char, _base);
}

int StringWrapper::serialize(MessageBuffer& buf) const {
  _smode.assert_can_serialize();
  JB_RETURN(buf.serialize_no_meta(_size));
  if (_base == nullptr) {
    assert(_size == MessageConst::NULL_PTR, "sanity");
    return 0;
  }
  assert(_base[_size] == '\0', "sanity");
  return buf.serialize_memcpy(_base, _size);
}

int StringWrapper::deserialize(MessageBuffer& buf) {
  _smode.assert_can_deserialize();
  assert(_base == nullptr, "can only be deserialized once");
  JB_RETURN(buf.deserialize_ref_no_meta(_size));
  if (_size == MessageConst::NULL_PTR) {
    return 0;
  }
  char* mem = NEW_C_HEAP_ARRAY(char, _size + 1, mtJBooster);
  JB_RETURN(buf.deserialize_memcpy(mem, _size));
  mem[_size] = '\0';
  _base = mem;
  return 0;
}

// -------------------------- FileWrapper for Serializer ---------------------------

FileWrapper::FileWrapper(const char* file_path, SerializationMode smode):
                         WrapperBase(smode),
                         _file_path(file_path), // don't have to be copied
                         _file_size(MessageConst::NULL_PTR),
                         _tmp_file_path(nullptr),
                         _fd(-1),
                         _errno(0),
                         _handled_file_size(0),
                         _handled_once(false) {
  if (smode == SerializationMode::SERIALIZE) {
    init_for_serialize();
  } else if (smode == SerializationMode::DESERIALIZE) {
    init_for_deserialize();
  } else {
    fatal("smode should not be SerializationMode::BOTH");
  }
}

FileWrapper::~FileWrapper() {
  if (has_file_err()) {
    if (_smode.can_serialize()) {
      log_trace(jbooster, serialization)("The file \"%s\" is serialized as null: error=%s(\"%s\").",
                _file_path, os::errno_name(_errno), os::strerror(_errno));
    } else {
      log_trace(jbooster, serialization)("The file \"%s\" is not deserialized (by this thread): error=%s(\"%s\").",
                _file_path, os::errno_name(_errno), os::strerror(_errno));
    }
  } else if (!is_file_all_handled()) {
    if (_smode.can_serialize()) {
      log_warning(jbooster, serialization)("The file \"%s\" is not fully serialized.",
                  _file_path);
    } else {
      log_warning(jbooster, serialization)("The file \"%s\" is not fully deserialized and will be deleted.",
                  _tmp_file_path);
    }
    if (_fd >= 0) {
      os::close(_fd);
      _fd = -1;
      if (_smode.can_deserialize()) {
        FileUtils::remove(_tmp_file_path);
      }
    }
  }
  guarantee(_fd < 0, "sanity");
  StringUtils::free_from_heap(_tmp_file_path);
}

void FileWrapper::init_for_serialize() {
  errno = 0;
  _fd = os::open(_file_path, O_BINARY | O_RDONLY, 0);
  _errno = errno;
  // We use lseek() instead of stat() to get the file size since we have opened the file.
  if (_errno == 0 && _fd >= 0) {
    _file_size = (uint32_t) os::lseek(_fd, 0, SEEK_END);
    os::lseek(_fd, 0, SEEK_SET);
  } else {
    assert(_fd < 0 && _errno != 0, "sanity");
  }
}

void FileWrapper::on_deser_tmp_file_opened() {
  // The tmp file is successfully created and opened.
  guarantee(_fd >= 0, "sanity");
  // Recheck if the target file already exists. Skip deserialization if the target file exists.
  if (FileUtils::is_file(_file_path)) {
    os::close(_fd);
    _fd = -1;
    FileUtils::remove(_tmp_file_path);
    log_info(jbooster, serialization)("The target file of \"%s\" already exists. Skipped deserialization.",
              _file_path);
    // Use is_tmp_file_already_exists() to check later.
    _errno = EEXIST;
  }
}

void FileWrapper::on_deser_tmp_file_exist() {
  // The tmp file already exists. Failed to open the file.
  // Use is_tmp_file_already_exists() to check later.
  log_info(jbooster, serialization)("The tmp file of \"%s\" already exists. Skipped deserialization.",
            _file_path);
}

void FileWrapper::on_deser_dir_not_exist() {
  // The folder does not exist. Create it.
  assert(_fd < 0, "sanity");
  const int dir_buf_len = strlen(_file_path) + 1;
  char* dir = NEW_C_HEAP_ARRAY(char, dir_buf_len, mtJBooster);
  const char* r = strrchr(_file_path, FileUtils::separator()[0]);
  if (r == nullptr) {
    snprintf(dir, dir_buf_len, ".%s", FileUtils::separator());
  } else {
    int len = r - _file_path;
    memcpy(dir, _file_path, len);
    dir[len] = '\0';
  }
  FileUtils::mkdirs(dir);
  FREE_C_HEAP_ARRAY(char, dir);
}

void FileWrapper::init_for_deserialize() {
  // Write to file "<file-path>.tmp" and then rename to "<file-path>".
  _tmp_file_path = JBoosterManager::calc_tmp_cache_path(_file_path);

  const int max_retry_times = 2;
  int retry_times = 0;
  while (++retry_times <= max_retry_times) {
    errno = 0;
    _fd = JBoosterManager::create_and_open_tmp_cache_file(_tmp_file_path);
    _errno = errno;
    switch (_errno) {
      case 0:
        on_deser_tmp_file_opened();
        return;
      case EEXIST:
        on_deser_tmp_file_exist();
        return;
      case ENOENT:
        // No need to try to make dirs twice.
        guarantee(retry_times == 1, "Failed to make dirs.");
        on_deser_dir_not_exist();
        break;
      default:
        fatal("failed to open file \"%s\": %s", _tmp_file_path, os::strerror(_errno));
        break;
    }
  }
}

uint32_t FileWrapper::get_size_to_send_this_time() const {
  assert(_fd >= 0, "sanity");
  return MIN2(MAX_SIZE_PER_TRANS, _file_size - _handled_file_size);
}

uint32_t FileWrapper::get_arg_meta_size_if_not_null() {
  return sizeof(_file_size) + sizeof(MAX_SIZE_PER_TRANS);
}

int FileWrapper::serialize(MessageBuffer& buf) const {
  _smode.assert_can_serialize();
  guarantee(!is_file_all_handled(), "the file is all parsed");

  // file size
  JB_RETURN(buf.serialize_no_meta(_file_size));
  if (_file_size == MessageConst::NULL_PTR) {
    assert(_fd < 0, "sanity");
    _handled_file_size = _file_size;
    _handled_once = true;
    return 0;
  }

  // size to handle this time
  uint32_t size_to_send = get_size_to_send_this_time();
  JB_RETURN(buf.serialize_no_meta(size_to_send));

  // content (use low-level APIs to save a memcpy)
  buf.expand_if_needed(buf.cur_offset() + size_to_send, buf.cur_offset());
  uint32_t left = size_to_send;
  do {
    ssize_t read_size_ret = os::read(_fd, buf.cur_buf_ptr(), left);
    if (read_size_ret == -1) {
      return errno;
    }
    uint32_t read_size = (uint32_t) read_size_ret;
    buf.skip_cur_offset(read_size);
    left -= read_size;
  } while (left > 0);

  // update status
  _handled_file_size += size_to_send;
  _handled_once = true;
  if (is_file_all_handled()) {
    os::close(_fd);
    _fd = -1;
  }
  return 0;
}

/**
 * The file to deserialize is null.
 */
void FileWrapper::on_deser_null() {
  if (!_handled_once && _handled_file_size == 0) {
    assert(_fd >= 0, "sanity");
    _handled_file_size = _file_size;
    _handled_once = true;
    os::close(_fd);
    _fd = -1;
    FileUtils::remove(_tmp_file_path);
  } else {
    guarantee(_handled_once && _handled_file_size == _file_size && _fd == -1, "sanity");
  }
}

/**
 * The whole file is all deserialized.
 */
void FileWrapper::on_deser_end() {
  assert(_fd >= 0, "sanity");
  os::close(_fd);
  _fd = -1;
  bool rename_successful = false;
  if (!FileUtils::is_file(_file_path)) {
    // We rename the tmp file to the target file finally if the file is deserialized successfully.
    // FileUtils::rename() may replace the target file by the tmp file if the target file exists,
    // which is not what we want.
    // Instead, we want the rename function to fail (atomically) when the target file already exists.
    // But we don't have to use some atomic function like renameat2 with RENAME_NOREPLACE here.
    // Because every target file is renamed from the tmp file so a target file cannot be created after
    // its tmp file is locked.
    chmod(_tmp_file_path, S_IREAD);
    rename_successful = FileUtils::rename(_tmp_file_path, _file_path);
    if (!rename_successful) {
      log_warning(jbooster, serialization)("Failed to rename the tmp file of \"%s\" "
                                            "and the tmp file will be removed. errno=%s(\"%s\").",
                                            _file_path, os::errno_name(errno), os::strerror(errno));
    }
  } else {
    // This branch should not be walked on.
    log_error(jbooster, serialization)("File \"%s\" already exists and will not be overwritten.", _file_path);
  }
  if (!rename_successful) {
    FileUtils::remove(_tmp_file_path);
  }
}

int FileWrapper::deserialize(MessageBuffer& buf) {
  _smode.assert_can_deserialize();
  assert(_fd >= 0, "the tmp file");
  guarantee(!is_file_all_handled(), "the deserialization is ended");

  // file size
  uint32_t file_size;
  JB_RETURN(buf.deserialize_ref_no_meta(file_size));
  if (_file_size != file_size) {
    if (_file_size == MessageConst::NULL_PTR) { // first transmission
      _file_size = file_size;
    } else {
      log_warning(jbooster, serialization)("File size mismatch while deserializing file \"%s\": "
                                           "expect_file_size=%u, actual_file_size=%u",
                                           _file_path, _file_size, file_size);
      return JBErr::BAD_ARG_DATA;
    }
  }
  if (_file_size == MessageConst::NULL_PTR) {
    on_deser_null();
    return 0;
  }

  // size to handle this time
  uint32_t size_to_recv;
  JB_RETURN(buf.deserialize_ref_no_meta(size_to_recv));

  // content (use low-level APIs to save a memcpy)
  if (!os::write(_fd, buf.cur_buf_ptr(), size_to_recv)) {
    int e = errno;
    errno = 0;
    guarantee(e != 0, "sanity");
    log_warning(jbooster, serialization)("Fail to write file \"%s\": errno=%s(\"%s\") .",
                                         _file_path, os::errno_name(e), os::strerror(e));
    JB_RETURN(e);
  }
  buf.skip_cur_offset(size_to_recv);

  // update status
  _handled_file_size += size_to_recv;
  _handled_once = true;
  if (is_file_all_handled()) {
    on_deser_end();
  }
  return 0;
}

bool FileWrapper::wait_for_file_deserialization(int timeout_millisecond) {
  jlong start = os::javaTimeMillis();
  do {
    if (FileUtils::is_file(_file_path)) {
      return true;
    }
    os::naked_short_sleep(50);
  } while ((os::javaTimeMillis() - start) < timeout_millisecond);
  log_warning(jbooster, serialization)("File \"%s\" is still being deserialized by another thread or process "
                                       "after %dms. Stop waiting.",
                                       _file_path, timeout_millisecond);
  return false;
}

int FileWrapper::send_file(CommunicationStream* stream) {
  // No need to check has_file_err() here because it's not an exception.
  // It's OK that the target file failed to be opened.
  while (!is_file_all_handled()) {
    bool ok = false;
    JB_RETURN(stream->send_request(MessageType::FileSegment, this));
    JB_RETURN(stream->recv_request(MessageType::FileSegment, &ok));
    guarantee(ok, "sanity");
  }
  return 0;
}

int FileWrapper::recv_file(CommunicationStream* stream) {
  // Check has_file_err() here and treat the err as an exception.
  // The tmp file should be opened.
  if (has_file_err()) return _errno;
  while (!is_file_all_handled()) {
    bool ok = true;
    JB_RETURN(stream->recv_request(MessageType::FileSegment, this));
    JB_RETURN(stream->send_request(MessageType::FileSegment, &ok));
  }
  return 0;
}
