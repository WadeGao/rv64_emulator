#pragma once

namespace rv64_emulator::errorcode {

enum class ErrorCode : int {
    kSuccess             = 0,
    kDramErrorCodeBase   = -100,
    kDecodeErrorCodeBase = -200,
    kCpuErrorCodeBase    = -300,
    kElfErrorCodeBase    = -400,
    kPlicErrorCodeBase   = -500,
};

enum class DramErrorCode : int {
    kSuccess                  = 0,
    kInvalidRange             = static_cast<int>(ErrorCode::kDramErrorCodeBase) - 1,
    kAccessBitsWidthUnaligned = static_cast<int>(ErrorCode::kDramErrorCodeBase) - 2,
};

enum class DecodeErrorCode : int {
    kSuccess        = 0,
    kFieldUndefined = static_cast<int>(ErrorCode::kDecodeErrorCodeBase) - 1,
};

enum class CpuErrorCode : int {
    kSuccess                 = 0,
    kExecuteFailure          = static_cast<int>(ErrorCode::kCpuErrorCodeBase) - 1,
    kReservedPrivilegeMode   = static_cast<int>(ErrorCode::kCpuErrorCodeBase) - 2,
    kTrapVectorModeUndefined = static_cast<int>(ErrorCode::kCpuErrorCodeBase) - 3,
};

enum class ElfErrorCode : int {
    kSuccess     = 0,
    kInvalidFile = static_cast<int>(ErrorCode::kElfErrorCodeBase) - 1,
};

enum class PlicErrorCode : int {
    kSuccess                   = 0,
    kAddrMisAlign              = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 1,
    kLoadFailure               = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 2,
    kStoreFailure              = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 3,
    kContextReadFailure        = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 4,
    kContextWriteFailure       = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 5,
    kPriorityReadFailure       = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 6,
    kPriorityWriteFailure      = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 7,
    kContextEnableReadFailure  = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 8,
    kContextEnableWriteFailure = static_cast<int>(ErrorCode::kPlicErrorCodeBase) - 9,
};

} // namespace rv64_emulator::errorcode
