#pragma once

namespace rv64_emulator::errorcode {

enum class ErrorCode : int {
    kSuccess             = 0,
    kDramErrorCodeBase   = -100,
    kDecodeErrorCodeBase = -200,
    kCpuErrorCodeBase    = -300,
    kElfErrorCodeBase    = -400,
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
    kSuccess      = 0,
    kInvalidFile = static_cast<int>(ErrorCode::kElfErrorCodeBase) - 1,
};

} // namespace rv64_emulator::errorcode
