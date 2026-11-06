// Host-side save snapshots. Does not modify or interpret Xbox save data.
// Invoked at startup only when explicitly enabled by the user.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace fable2::native_saves {
namespace fs = std::filesystem;

enum class SnapshotResult { kNoSaves, kCreated, kSkipped, kFailed };

inline SnapshotResult SnapshotAtStartup(const fs::path& game_dir) {
  constexpr std::uintmax_t kMaxBytes = 256ull * 1024 * 1024;
  constexpr std::size_t kMaxFiles = 4096;
  const fs::path source = game_dir / "saves";
  const fs::path backups = game_dir / "save_backups";
  std::error_code ec;
  if (!fs::is_directory(source, ec)) return SnapshotResult::kNoSaves;

  // First enumerate and bound the snapshot before changing any backup.
  std::vector<fs::path> relative_files;
  std::uintmax_t total = 0;
  fs::recursive_directory_iterator it(
      source, fs::directory_options::none, ec);
  const fs::recursive_directory_iterator end;
  if (ec) return SnapshotResult::kFailed;
  for (; it != end; it.increment(ec)) {
    if (ec) return SnapshotResult::kFailed;
    const auto status = it->symlink_status(ec);
    if (ec) return SnapshotResult::kFailed;
    // Never follow links out of the game directory or preserve reparse points.
    if (fs::is_symlink(status)) return SnapshotResult::kFailed;
    if (fs::is_directory(status)) continue;
    if (!fs::is_regular_file(status)) return SnapshotResult::kFailed;
    const auto bytes = it->file_size(ec);
    if (ec || bytes > kMaxBytes - total ||
        relative_files.size() >= kMaxFiles) return SnapshotResult::kSkipped;
    total += bytes;
    relative_files.push_back(it->path().lexically_relative(source));
  }
  if (ec) return SnapshotResult::kFailed;
  if (relative_files.empty()) return SnapshotResult::kNoSaves;

  // Keep the last three complete snapshots. The pending directory is never
  // treated as a restorable snapshot, even if a previous run was interrupted.
  fs::create_directories(backups, ec);
  if (ec) return SnapshotResult::kFailed;
  const fs::path pending = backups / "pending";
  fs::remove_all(pending, ec);
  if (ec) return SnapshotResult::kFailed;
  fs::create_directory(pending, ec);
  if (ec) return SnapshotResult::kFailed;
  for (const auto& rel : relative_files) {
    const auto destination = pending / rel;
    fs::create_directories(destination.parent_path(), ec);
    if (ec) break;
    fs::copy_file(source / rel, destination, fs::copy_options::none, ec);
    if (ec) break;
  }
  if (ec) {
    std::error_code cleanup;
    fs::remove_all(pending, cleanup);
    return SnapshotResult::kFailed;
  }
  fs::remove_all(backups / "snapshot_3", ec);
  if (!ec && fs::exists(backups / "snapshot_2", ec))
    fs::rename(backups / "snapshot_2", backups / "snapshot_3", ec);
  if (!ec && fs::exists(backups / "snapshot_1", ec))
    fs::rename(backups / "snapshot_1", backups / "snapshot_2", ec);
  if (!ec) fs::rename(pending, backups / "snapshot_1", ec);
  return ec ? SnapshotResult::kFailed : SnapshotResult::kCreated;
}
}  // namespace fable2::native_saves
