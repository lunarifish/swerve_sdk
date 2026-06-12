#pragma once

#include <cstdint>
#include <cstring>

#include "crc.h"
#include "w25q64.h"

/**
 * @brief 基于 W25Q64 Flash 的结构化配置存储（A/B 双扇区）。
 *
 * 使用两个 4KB 扇区交替写入，Commit() 总是写到非活跃扇区，
 * 确保写入中途断电不会损坏已有数据。
 *
 * 单扇区布局 (4096 bytes):
 * ┌────────┬──────┬────────────────────────────────────┐
 * │ Offset │ Size │ Field                              │
 * ├────────┼──────┼────────────────────────────────────┤
 * │      0 │    4 │ Magic: 0x50495045 ("PIPE")         │
 * │      4 │    4 │ Format version (当前 = 1)           │
 * │      8 │    4 │ Write counter (单调递增，判断新旧)   │
 * │     12 │    4 │ Data CRC32 (覆盖 [20..20+data_size])│
 * │     16 │    4 │ Data size (= sizeof(T))             │
 * │     20 │ 4076 │ User config struct (packed)         │
 * └────────┴──────┴────────────────────────────────────┘
 *
 * @tparam T 用户配置结构体，须为 packed 且 sizeof(T) ≤ 4076
 *
 * Usage:
 * @code
 *   struct __attribute__((packed)) MyConfig {
 *     uint32_t schema_version;
 *     float pd_kp[5];
 *     float pd_kd[5];
 *     // ...
 *   };
 *
 *   FlashPreferences<MyConfig> prefs{&hospi2, 0x7FE000};
 *   prefs.Begin();
 *   prefs.data().pd_kp[0] = 12.5f;
 *   prefs.Commit();  // 写扇区 B
 *   prefs.data().pd_kp[0] = 15.0f;
 *   prefs.Commit();  // 写扇区 A（交替）
 * @endcode
 */
template <typename T>
class FlashPreferences {
 public:
  static constexpr size_t kSectorSize = 4096;
  static constexpr size_t kHeaderSize = 20;
  static constexpr size_t kMaxDataSize = kSectorSize - kHeaderSize;

  static_assert(sizeof(T) <= kMaxDataSize, "Config struct too large for one 4KB sector");

  /**
   * @param hospi          OSPI 句柄指针
   * @param sector_a_addr  扇区 A 基地址（须 4KB 对齐）
   * @param sector_b_addr  扇区 B 基地址（须 4KB 对齐；默认 = sector_a + 4KB）
   */
  FlashPreferences(OSPI_HandleTypeDef *hospi, uint32_t sector_a_addr, uint32_t sector_b_addr = 0)
      : hospi_(hospi),
        sector_a_(sector_a_addr),
        sector_b_(sector_b_addr ? sector_b_addr : sector_a_addr + kSectorSize) {}

  /**
   * @brief 扫描两个扇区，加载最新有效数据到缓存。
   *        若两扇区均无效则自动格式化。
   */
  bool Begin() {
    SectorInfo info_a = ReadSector(sector_a_);
    SectorInfo info_b = ReadSector(sector_b_);

    bool a_valid = info_a.valid;
    bool b_valid = info_b.valid;

    if (a_valid && b_valid) {
      if (info_a.write_counter >= info_b.write_counter) {
        LoadFrom(info_a);
        active_is_a_ = true;
      } else {
        LoadFrom(info_b);
        active_is_a_ = false;
      }
    } else if (a_valid) {
      LoadFrom(info_a);
      active_is_a_ = true;
    } else if (b_valid) {
      LoadFrom(info_b);
      active_is_a_ = false;
    } else {
      std::memset(&cached_data_, 0, sizeof(T));
      write_counter_ = 0;
      active_is_a_ = false;
      if (!FormatSector(sector_a_) || !FormatSector(sector_b_)) {
        dirty_ = false;
        valid_ = false;
        return false;
      }
    }

    dirty_ = false;
    valid_ = true;
    return true;
  }

  /** @brief 获取配置数据的可变引用（修改后需 Commit() 落盘） */
  T &data() {
    dirty_ = true;
    return cached_data_;
  }
  const T &data() const { return cached_data_; }

  /** @brief 当前写入计数器 */
  uint32_t write_counter() const { return write_counter_; }

  /** @brief 检查是否已成功 Begin() */
  bool IsValid() const { return valid_; }

  /**
   * @brief 将当前缓存写入非活跃扇区。
   *        写入中途断电不会损坏已有数据。
   * @return true 写入成功
   */
  bool Commit() {
    if (!valid_) return false;
    if (!dirty_) return true;

    uint32_t target_addr = active_is_a_ ? sector_b_ : sector_a_;
    write_counter_++;

    if (!WriteSector(target_addr, write_counter_, cached_data_)) {
      write_counter_--;
      return false;
    }

    // 回读校验：确保写入的数据完整一致
    SectorInfo verify = ReadSector(target_addr);
    if (!verify.valid || verify.write_counter != write_counter_) {
      write_counter_--;
      return false;
    }
    if (std::memcmp(verify.raw_data, &cached_data_, sizeof(T)) != 0) {
      write_counter_--;
      return false;
    }

    active_is_a_ = !active_is_a_;
    dirty_ = false;
    return true;
  }

  /** @brief 擦除两个扇区并重置 */
  bool Clear() {
    if (!FormatSector(sector_a_) || !FormatSector(sector_b_)) {
      return false;
    }
    std::memset(&cached_data_, 0, sizeof(T));
    write_counter_ = 0;
    active_is_a_ = false;
    dirty_ = false;
    valid_ = true;
    return true;
  }

 private:
  static constexpr uint32_t kMagic = 0x50495045;
  static constexpr uint32_t kFormatVersion = 1;

  struct SectorInfo {
    bool valid{false};
    uint32_t write_counter{0};
    uint8_t raw_data[kMaxDataSize]{};
  };

  static uint32_t ComputeCRC32(const uint8_t *data, size_t len) {
    if (len == 0) return 0xFFFFFFFF;

    size_t word_count = (len + 3) / 4;
    uint32_t word_buf[1024];
    std::memset(word_buf, 0, word_count * sizeof(uint32_t));
    std::memcpy(word_buf, data, len);

    return HAL_CRC_Calculate(&hcrc, word_buf, word_count);
  }

  SectorInfo ReadSector(uint32_t addr) const {
    SectorInfo info{};

    uint8_t raw[kSectorSize];
    if (OSPI_W25Qxx_ReadBuffer(raw, addr, kSectorSize) != OSPI_W25Qxx_OK) return info;

    uint32_t magic, format_version, data_size;
    std::memcpy(&magic, &raw[0], 4);
    std::memcpy(&format_version, &raw[4], 4);
    std::memcpy(&info.write_counter, &raw[8], 4);
    std::memcpy(&data_size, &raw[16], 4);

    if (magic != kMagic) return info;
    if (format_version != kFormatVersion) return info;
    if (data_size != sizeof(T)) return info;
    if (data_size > kMaxDataSize) return info;

    uint32_t stored_crc;
    std::memcpy(&stored_crc, &raw[12], 4);
    uint32_t computed_crc = ComputeCRC32(&raw[kHeaderSize], data_size);
    if (stored_crc != computed_crc) return info;

    std::memcpy(info.raw_data, &raw[kHeaderSize], data_size);
    info.valid = true;
    return info;
  }

  void LoadFrom(const SectorInfo &info) {
    std::memcpy(&cached_data_, info.raw_data, sizeof(T));
    write_counter_ = info.write_counter;
  }

  bool WriteSector(uint32_t addr, uint32_t write_counter, const T &data) {
    uint8_t sector[kSectorSize];
    std::memset(sector, 0xFF, kSectorSize);

    uint32_t magic = kMagic;
    uint32_t format_version = kFormatVersion;
    uint32_t data_size = sizeof(T);

    std::memcpy(&sector[0], &magic, 4);
    std::memcpy(&sector[4], &format_version, 4);
    std::memcpy(&sector[8], &write_counter, 4);
    std::memcpy(&sector[16], &data_size, 4);

    std::memcpy(&sector[kHeaderSize], &data, data_size);

    uint32_t crc = ComputeCRC32(&sector[kHeaderSize], data_size);
    std::memcpy(&sector[12], &crc, 4);

    if (OSPI_W25Qxx_SectorErase(addr) != OSPI_W25Qxx_OK) return false;
    if (OSPI_W25Qxx_WriteBuffer(sector, addr, kSectorSize) != OSPI_W25Qxx_OK) return false;
    return true;
  }

  bool FormatSector(uint32_t addr) {
    return OSPI_W25Qxx_SectorErase(addr) == OSPI_W25Qxx_OK;
  }

  OSPI_HandleTypeDef *hospi_;
  uint32_t sector_a_;
  uint32_t sector_b_;
  T cached_data_{};
  uint32_t write_counter_{0};
  bool active_is_a_{false};
  bool dirty_{false};
  bool valid_{false};
};
