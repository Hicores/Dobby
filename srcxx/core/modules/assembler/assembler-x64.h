#ifndef CORE_MODULES_ASSEMBLER_X64_ASSEMBLER_H_
#define CORE_MODULES_ASSEMBLER_X64_ASSEMBLER_H_

#include "core/arch/x64/constants-x64.h"
#include "core/arch/x64/registers-x64.h"

#include "core/modules/assembler/assembler.h"

#include <assert.h>

namespace zz {
namespace x64 {

#define IsInt8(imm) ((2 ^ 8) > imm)

class PseudoLabel : public Label {
public:
};

class Immediate {
public:
  explicit Immediate(int64_t imm) : value_(imm) {
  }

  int64_t value() const {
    return value_;
  }

private:
  const int64_t value_;
};

class Operand {
public:
  // [base]
  Operand(Register base);

  // [base + disp/r]
  Operand(Register base, int32_t disp);

  // [base + index*scale + disp/r]
  Operand(Register base, Register index, ScaleFactor scale, int32_t disp);

  // [index*scale + disp/r]
  Operand(Register index, ScaleFactor scale, int32_t disp);

public: // Getter and Setter
  uint8_t rex() const {
    return rex_;
  }

  inline uint8_t rex_b() const {
    return (rex_ & REX_B);
  }

  inline uint8_t rex_x() const {
    return (rex_ & REX_X);
  }

  inline uint8_t rex_r() const {
    return (rex_ & REX_R);
  }

  inline uint8_t rex_w() const {
    return (rex_ & REX_W);
  }

  uint8_t mod() const {
    return (encoding_at(0) >> 6) & 3;
  }

  Register rm() const {
    int rm_rex = rex_b() << 3;
    return Register::from_code(rm_rex + (encoding_at(0) & 7));
  }

  ScaleFactor scale() const {
    return static_cast<ScaleFactor>((encoding_at(1) >> 6) & 3);
  }

  Register index() const {
    int index_rex = rex_x() << 2;
    return Register::from_code(index_rex + ((encoding_at(1) >> 3) & 7));
  }

  Register base() const {
    int base_rex = rex_b() << 3;
    return Register::from_code(base_rex + (encoding_at(1) & 7));
  }

  int8_t disp8() const {
    ASSERT(length_ >= 2);
    return static_cast<int8_t>(encoding_[length_ - 1]);
  }

  int32_t disp32() const {
    ASSERT(length_ >= 5);
    return static_cast<int32_t>(encoding_[length_ - 4]);
  }

protected:
  Operand() : length_(0), rex_(REX_NONE) {
  } // Needed by subclass Address.

  void SetModRM(int mod, Register rm) {
    ASSERT((mod & ~3) == 0);
    if ((rm.code() > 7) && !((rm.Is(r12)) && (mod != 3))) {
      rex_ |= REX_B;
    }
    encoding_[0] = (mod << 6) | (rm.code() & 7);
    length_      = 1;
  }

  void SetSIB(ScaleFactor scale, Register index, Register base) {
    ASSERT(length_ == 1);
    ASSERT((scale & ~3) == 0);
    if (base.code() > 7) {
      ASSERT((rex_ & REX_B) == 0); // Must not have REX.B already set.
      rex_ |= REX_B;
    }
    if (index.code() > 7)
      rex_ |= REX_X;
    encoding_[1] = (scale << 6) | ((index.code() & 7) << 3) | (base.code() & 7);
    length_      = 2;
  }

  void SetDisp8(int8_t disp) {
    ASSERT(length_ == 1 || length_ == 2);
    encoding_[length_++] = static_cast<uint8_t>(disp);
  }

  void SetDisp32(int32_t disp) {
    ASSERT(length_ == 1 || length_ == 2);
    *(int32_t *)&encoding_[length_] = disp;
    length_ += sizeof(disp);
  }

private:
  // explicit Operand(Register reg) : rex_(REX_NONE) { SetModRM(3, reg); }

  // Get the operand encoding byte at the given index.
  uint8_t encoding_at(intptr_t index) const {
    ASSERT(index >= 0 && index < length_);
    return encoding_[index];
  }

private:
  uint8_t length_;
  uint8_t rex_;
  uint8_t encoding_[6];
};

class Address : public Operand {
public:
  Address(Register base, int32_t disp) {
    int base_ = base.code();
    int rbp_  = rbp.code();
    int rsp_  = rsp.code();
    if ((disp == 0) && ((base_ & 7) != rbp_)) {
      SetModRM(0, base);
      if ((base_ & 7) == rsp_) {
        SetSIB(TIMES_1, rsp, base);
      }
    } else if (IsInt8(disp)) {
      SetModRM(1, base);
      if ((base_ & 7) == rsp_) {
        SetSIB(TIMES_1, rsp, base);
      }
      SetDisp8(disp);
    } else {
      SetModRM(2, base);
      if ((base_ & 7) == rsp_) {
        SetSIB(TIMES_1, rsp, base);
      }
      SetDisp32(disp);
    }
  }

  // This addressing mode does not exist.
  Address(Register base, Register r);

  Address(Register index, ScaleFactor scale, int32_t disp) {
    ASSERT(index != rsp); // Illegal addressing mode.
    SetModRM(0, rsp);
    SetSIB(scale, index, rbp);
    SetDisp32(disp);
  }

  // This addressing mode does not exist.
  Address(Register index, ScaleFactor scale, Register r);

  Address(Register base, Register index, ScaleFactor scale, int32_t disp) {
    ASSERT(index != rsp); // Illegal addressing mode.
    int rbp_ = rbp.code();
    if ((disp == 0) && ((base.code() & 7) != rbp_)) {
      SetModRM(0, rsp);
      SetSIB(scale, index, base);
    } else if (IsInt8(disp)) {
      SetModRM(1, rsp);
      SetSIB(scale, index, base);
      SetDisp8(disp);
    } else {
      SetModRM(2, rsp);
      SetSIB(scale, index, base);
      SetDisp32(disp);
    }
  }

  // This addressing mode does not exist.
  Address(Register base, Register index, ScaleFactor scale, Register r);

private:
  Address(Register base, int32_t disp, bool fixed) {
    ASSERT(fixed);
    SetModRM(2, base);
    if ((base.code() & 7) == rsp.code()) {
      SetSIB(TIMES_1, rsp, base);
    }
    SetDisp32(disp);
  }
};

class Assembler : public AssemblerBase {
public:
  void Emit1(byte val) {
  }

  void pushfq() {
    Emit1(0x9C);
  }


};

class TurboAssembler : public Assembler {
public:
  addr_t CurrentIP() {
    return pc_offset() + (addr_t )realized_address_;
  }
};

} // namespace x64
} // namespace zz

#endif
