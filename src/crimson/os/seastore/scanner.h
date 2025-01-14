// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-
// vim: ts=8 sw=2 smarttab expandtab

#pragma once

#include "crimson/common/errorator.h"
#include "crimson/os/seastore/seastore_types.h"
#include "crimson/os/seastore/segment_manager.h"

namespace crimson::os::seastore {

class SegmentCleaner;

class Scanner {
public:
  using read_ertr = crimson::errorator<
    crimson::ct_error::input_output_error,
    crimson::ct_error::invarg,
    crimson::ct_error::enoent,
    crimson::ct_error::erange>;

  Scanner(SegmentManager& segment_manager)
    : segment_manager(segment_manager) {}
  using read_segment_header_ertr = crimson::errorator<
    crimson::ct_error::enoent,
    crimson::ct_error::enodata,
    crimson::ct_error::input_output_error
    >;
  using read_segment_header_ret = read_segment_header_ertr::future<
    segment_header_t>;
  read_segment_header_ret read_segment_header(segment_id_t segment);

  /**
   * scan_extents
   *
   * Scans records beginning at addr until the first record boundary after
   * addr + bytes_to_read.
   *
   * Returns list<extent, extent_info>
   * cursor.is_complete() will be true when no further extents exist in segment.
   */
  using scan_extents_cursor = scan_valid_records_cursor;
  using scan_extents_ertr = read_ertr::extend<crimson::ct_error::enodata>;
  using scan_extents_ret_bare = std::list<std::pair<paddr_t, extent_info_t>>;
  using scan_extents_ret = scan_extents_ertr::future<scan_extents_ret_bare>;
  scan_extents_ret scan_extents(
    scan_extents_cursor &cursor,
    extent_len_t bytes_to_read
  );

  using scan_valid_records_ertr = read_ertr::extend<crimson::ct_error::enodata>;
  using scan_valid_records_ret = scan_valid_records_ertr::future<
    size_t>;
  using found_record_handler_t = std::function<
    scan_valid_records_ertr::future<>(
      paddr_t record_block_base,
      // callee may assume header and bl will remain valid until
      // returned future resolves
      const record_header_t &header,
      const bufferlist &bl)>;
  scan_valid_records_ret scan_valid_records(
    scan_valid_records_cursor &cursor, ///< [in, out] cursor, updated during call
    segment_nonce_t nonce,             ///< [in] nonce for segment
    size_t budget,                     ///< [in] max budget to use
    found_record_handler_t &handler    ///< [in] handler for records
  ); ///< @return used budget

private:
  SegmentManager& segment_manager;

  /// read record metadata for record starting at start
  using read_validate_record_metadata_ertr = read_ertr;
  using read_validate_record_metadata_ret =
    read_validate_record_metadata_ertr::future<
      std::optional<std::pair<record_header_t, bufferlist>>
    >;
  read_validate_record_metadata_ret read_validate_record_metadata(
    paddr_t start,
    segment_nonce_t nonce);

  /// attempts to decode extent infos from bl, return nullopt if unsuccessful
  std::optional<std::vector<extent_info_t>> try_decode_extent_infos(
    record_header_t header,
    const bufferlist &bl);

  /// read and validate data
  using read_validate_data_ertr = read_ertr;
  using read_validate_data_ret = read_validate_data_ertr::future<bool>;
  read_validate_data_ret read_validate_data(
    paddr_t record_base,
    const record_header_t &header  ///< caller must ensure lifetime through
                                   ///  future resolution
  );

  /// validate embedded metadata checksum
  static bool validate_metadata(const bufferlist &bl);

};

using ScannerRef = std::unique_ptr<Scanner>;

} // namespace crimson::os::seastore
