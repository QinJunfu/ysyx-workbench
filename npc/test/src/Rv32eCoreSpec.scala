package npc

import chisel3._
import chisel3.simulator.EphemeralSimulator._
import org.scalatest.freespec.AnyFreeSpec
import org.scalatest.matchers.must.Matchers

import scala.collection.mutable

class Rv32eCoreSpec extends AnyFreeSpec with Matchers {
  private val Mask32 = (BigInt(1) << 32) - 1
  private val Ebreak = BigInt("00100073", 16)
  private val Nop    = BigInt("00000013", 16)

  private def unsigned(value: Int, width: Int): BigInt = BigInt(value) & ((BigInt(1) << width) - 1)

  private def i(imm: Int, rs1: Int, funct3: Int, rd: Int, opcode: Int = 0x13): BigInt =
    (unsigned(imm, 12) << 20) | (BigInt(rs1) << 15) | (BigInt(funct3) << 12) | (BigInt(rd) << 7) | opcode

  private def s(imm: Int, rs2: Int, rs1: Int, funct3: Int): BigInt = {
    val value = unsigned(imm, 12)
    (value >> 5 << 25) | (BigInt(rs2) << 20) | (BigInt(rs1) << 15) | (BigInt(funct3) << 12) |
      ((value & 0x1f) << 7) | 0x23
  }

  private def r(funct7: Int, rs2: Int, rs1: Int, funct3: Int, rd: Int): BigInt =
    (BigInt(funct7) << 25) | (BigInt(rs2) << 20) | (BigInt(rs1) << 15) | (BigInt(funct3) << 12) |
      (BigInt(rd) << 7) | 0x33

  private def u(imm20: Int, rd: Int, opcode: Int): BigInt =
    (unsigned(imm20, 20) << 12) | (BigInt(rd) << 7) | opcode

  private def b(imm: Int, rs2: Int, rs1: Int, funct3: Int): BigInt = {
    require((imm & 1) == 0, "branch displacement must be even")
    val value = unsigned(imm, 13)
    ((value >> 12) << 31) | (((value >> 5) & 0x3f) << 25) | (BigInt(rs2) << 20) |
      (BigInt(rs1) << 15) | (BigInt(funct3) << 12) | (((value >> 1) & 0xf) << 8) |
      (((value >> 11) & 1) << 7) | 0x63
  }

  private def j(imm: Int, rd: Int): BigInt = {
    require((imm & 1) == 0, "jump displacement must be even")
    val value = unsigned(imm, 21)
    ((value >> 20) << 31) | (((value >> 1) & 0x3ff) << 21) | (((value >> 11) & 1) << 20) |
      (((value >> 12) & 0xff) << 12) | (BigInt(rd) << 7) | 0x6f
  }

  private def csr(csrAddr: Int, rs1OrZimm: Int, funct3: Int, rd: Int): BigInt =
    (BigInt(csrAddr) << 20) | (BigInt(rs1OrZimm) << 15) | (BigInt(funct3) << 12) | (BigInt(rd) << 7) | 0x73

  private def addi(rd: Int, rs1: Int, imm: Int): BigInt = i(imm, rs1, 0, rd)
  private def lbu(rd: Int, rs1: Int, imm: Int): BigInt = i(imm, rs1, 4, rd, 0x03)
  private def lw(rd: Int, rs1: Int, imm: Int): BigInt = i(imm, rs1, 2, rd, 0x03)
  private def sw(rs2: Int, rs1: Int, imm: Int): BigInt = s(imm, rs2, rs1, 2)
  private def sb(rs2: Int, rs1: Int, imm: Int): BigInt = s(imm, rs2, rs1, 0)
  private def sh(rs2: Int, rs1: Int, imm: Int): BigInt = s(imm, rs2, rs1, 1)
  private def jalr(rd: Int, rs1: Int, imm: Int): BigInt = i(imm, rs1, 0, rd, 0x67)

  private final case class Commit(
      pc: BigInt,
      dnpc: BigInt,
      gprs: Vector[BigInt],
      read0: Boolean,
      read1: Boolean,
      write0: Boolean,
      write0Addr: BigInt,
      write0Data: BigInt,
      write0Mask: BigInt,
      write1: Boolean,
      write1Addr: BigInt,
      write1Data: BigInt,
      write1Mask: BigInt,
      halt: Boolean,
      invalid: Boolean
  )

  private val gprPorts = (dut: Rv32eCore) => Vector(
    dut.io.retireGpr0,
    dut.io.retireGpr1,
    dut.io.retireGpr2,
    dut.io.retireGpr3,
    dut.io.retireGpr4,
    dut.io.retireGpr5,
    dut.io.retireGpr6,
    dut.io.retireGpr7,
    dut.io.retireGpr8,
    dut.io.retireGpr9,
    dut.io.retireGpr10,
    dut.io.retireGpr11,
    dut.io.retireGpr12,
    dut.io.retireGpr13,
    dut.io.retireGpr14,
    dut.io.retireGpr15
  )

  private def wordAt(memory: mutable.Map[BigInt, Int], address: BigInt): BigInt =
    (0 until 4).foldLeft(BigInt(0)) { (word, byteIndex) =>
      word | (BigInt(memory.getOrElse(address + byteIndex, 0) & 0xff) << (byteIndex * 8))
    }

  private def applyWrite(memory: mutable.Map[BigInt, Int], address: BigInt, data: BigInt, mask: BigInt): Unit =
    for (byteIndex <- 0 until 4 if ((mask >> byteIndex) & 1) == 1) {
      memory(address + byteIndex) = ((data >> (byteIndex * 8)) & 0xff).toInt
    }

  private def resetCore(dut: Rv32eCore): Unit = {
    dut.io.imemData.poke(Nop.U(32.W))
    dut.io.dmemRdata.poke(0.U(32.W))
    dut.io.dmemRdata2.poke(0.U(32.W))
    dut.reset.poke(true.B)
    dut.clock.step()
    dut.reset.poke(false.B)
  }

  private def executeOne(dut: Rv32eCore, instruction: BigInt, memory: mutable.Map[BigInt, Int]): Commit = {
    dut.io.imemData.poke((instruction & Mask32).U(32.W))
    dut.io.dmemRdata.poke(0.U(32.W))
    dut.io.dmemRdata2.poke(0.U(32.W))

    if (dut.io.dmemReadValid.peek().litToBoolean) {
      dut.io.dmemRdata.poke(wordAt(memory, dut.io.dmemAddr.peek().litValue).U(32.W))
    }
    if (dut.io.dmemReadValid2.peek().litToBoolean) {
      dut.io.dmemRdata2.poke(wordAt(memory, dut.io.dmemAddr2.peek().litValue).U(32.W))
    }

    val commit = Commit(
      pc = dut.io.retirePc.peek().litValue,
      dnpc = dut.io.retireDnPc.peek().litValue,
      gprs = gprPorts(dut).map(_.peek().litValue),
      read0 = dut.io.dmemReadValid.peek().litToBoolean,
      read1 = dut.io.dmemReadValid2.peek().litToBoolean,
      write0 = dut.io.dmemWrite0Valid.peek().litToBoolean,
      write0Addr = dut.io.dmemWrite0Addr.peek().litValue,
      write0Data = dut.io.dmemWrite0Data.peek().litValue,
      write0Mask = dut.io.dmemWrite0Mask.peek().litValue,
      write1 = dut.io.dmemWrite1Valid.peek().litToBoolean,
      write1Addr = dut.io.dmemWrite1Addr.peek().litValue,
      write1Data = dut.io.dmemWrite1Data.peek().litValue,
      write1Mask = dut.io.dmemWrite1Mask.peek().litValue,
      halt = dut.io.halt.peek().litToBoolean,
      invalid = dut.io.invalid.peek().litToBoolean
    )

    if (commit.write0) {
      applyWrite(memory, commit.write0Addr, commit.write0Data, commit.write0Mask)
    }
    if (commit.write1) {
      applyWrite(memory, commit.write1Addr, commit.write1Data, commit.write1Mask)
    }
    dut.clock.step()
    commit
  }

  private def runProgram(
      dut: Rv32eCore,
      program: Map[Int, BigInt],
      memory: mutable.Map[BigInt, Int],
      maxCycles: Int = 128
  ): Vector[Commit] = {
    val commits = Vector.newBuilder[Commit]
    for (_ <- 0 until maxCycles) {
      val pc = dut.io.imemAddr.peek().litValue
      val commit = executeOne(dut, program.getOrElse(pc.toInt, Ebreak), memory)
      commits += commit
      if (commit.halt) {
        return commits.result()
      }
    }
    fail(s"program did not halt within $maxCycles cycles")
  }

  "the RV32E core" - {
    "runs the eight minirv instructions while keeping x0 fixed" in {
      simulate(new Rv32eCore(0)) { dut =>
        val memory = mutable.Map.empty[BigInt, Int]
        val program = Map[Int, BigInt](
          0 -> addi(0, 0, 1),
          4 -> addi(1, 0, 0x100),
          8 -> u(0x12345, 2, 0x37),
          12 -> addi(2, 2, 0x678),
          16 -> sw(2, 1, 0),
          20 -> lbu(3, 1, 1),
          24 -> lw(4, 1, 0),
          28 -> r(0, 3, 4, 0, 5),
          32 -> addi(6, 0, 0x7f),
          36 -> sb(6, 1, 3),
          40 -> jalr(7, 0, 48),
          44 -> addi(8, 0, 42),
          48 -> Ebreak
        )

        resetCore(dut)
        val commits = runProgram(dut, program, memory)
        val last = commits.last
        last.halt mustBe true
        last.invalid mustBe false
        last.gprs(0) mustBe 0
        last.gprs(2) mustBe BigInt("12345678", 16)
        last.gprs(3) mustBe BigInt("56", 16)
        last.gprs(4) mustBe BigInt("12345678", 16)
        last.gprs(5) mustBe BigInt("123456ce", 16)
        last.gprs(7) mustBe 44
        last.gprs(8) mustBe 0
        wordAt(memory, 0x100) mustBe BigInt("7f345678", 16)
      }
    }

    "splits unaligned word stores and joins unaligned word loads" in {
      simulate(new Rv32eCore(0)) { dut =>
        val memory = mutable.Map[BigInt, Int](BigInt(0x100) -> 0xaa)
        val program = Map[Int, BigInt](
          0 -> addi(1, 0, 0x101),
          4 -> u(0x11223, 2, 0x37),
          8 -> addi(2, 2, 0x344),
          12 -> sw(2, 1, 0),
          16 -> lw(3, 1, 0),
          20 -> Ebreak
        )

        resetCore(dut)
        val commits = runProgram(dut, program, memory)
        val store = commits.find(_.pc == 12).getOrElse(fail("store instruction did not retire"))
        store.write0 mustBe true
        store.write0Addr mustBe 0x100
        store.write0Data mustBe BigInt("22334400", 16)
        store.write0Mask mustBe BigInt("e", 16)
        store.write1 mustBe true
        store.write1Addr mustBe 0x104
        store.write1Data mustBe BigInt("11", 16)
        store.write1Mask mustBe 1

        val load = commits.find(_.pc == 16).getOrElse(fail("load instruction did not retire"))
        load.read0 mustBe true
        load.read1 mustBe true
        commits.last.gprs(3) mustBe BigInt("11223344", 16)
        wordAt(memory, 0x100) mustBe BigInt("223344aa", 16)
        wordAt(memory, 0x104) mustBe BigInt("11", 16)
      }
    }

    "implements arithmetic, branches, JAL, JALR, and EBREAK" in {
      simulate(new Rv32eCore(0)) { dut =>
        val memory = mutable.Map.empty[BigInt, Int]
        val program = Map[Int, BigInt](
          0 -> addi(1, 0, -2),
          4 -> addi(2, 0, 1),
          8 -> r(0, 2, 1, 2, 3), // SLT
          12 -> r(0, 2, 1, 3, 4), // SLTU
          16 -> r(0x20, 1, 2, 0, 5), // SUB
          20 -> i(0x401, 1, 5, 6), // SRAI x6, x1, 1
          24 -> i(4, 2, 1, 7), // SLLI
          28 -> i(0xff, 7, 4, 8), // XORI
          32 -> i(0x11, 0, 6, 9), // ORI
          36 -> i(0x0f, 8, 7, 10), // ANDI
          40 -> b(8, 2, 1, 4), // BLT, skips PC 44
          44 -> addi(11, 0, 99),
          48 -> j(8, 12),
          52 -> addi(13, 0, 77),
          56 -> jalr(14, 0, 64),
          60 -> addi(15, 0, 88),
          64 -> Ebreak
        )

        resetCore(dut)
        val commits = runProgram(dut, program, memory)
        val last = commits.last
        last.halt mustBe true
        last.gprs(3) mustBe 1
        last.gprs(4) mustBe 0
        last.gprs(5) mustBe 3
        last.gprs(6) mustBe Mask32
        last.gprs(7) mustBe 16
        last.gprs(8) mustBe 239
        last.gprs(9) mustBe 17
        last.gprs(10) mustBe 15
        last.gprs(11) mustBe 0
        last.gprs(12) mustBe 52
        last.gprs(13) mustBe 0
        last.gprs(14) mustBe 60
        last.gprs(15) mustBe 0
      }
    }

    "supports the minimal Zicsr ECALL/MRET path" in {
      simulate(new Rv32eCore(0)) { dut =>
        val memory = mutable.Map.empty[BigInt, Int]
        val program = Map[Int, BigInt](
          0 -> csr(0x305, 16, 5, 0), // CSRRWI x0, mtvec, 16
          4 -> BigInt("00000073", 16), // ECALL
          16 -> csr(0x342, 0, 2, 1), // CSRRS x1, mcause, x0
          20 -> addi(2, 0, 32),
          24 -> csr(0x341, 2, 1, 0), // CSRRW x0, mepc, x2
          28 -> BigInt("30200073", 16), // MRET
          32 -> Ebreak
        )

        resetCore(dut)
        val commits = runProgram(dut, program, memory)
        commits.last.halt mustBe true
        commits.last.gprs(1) mustBe 11
      }
    }

    "reports RV32I-only register encodings as invalid and stops after their retire event" in {
      simulate(new Rv32eCore(0)) { dut =>
        val memory = mutable.Map.empty[BigInt, Int]
        resetCore(dut)
        val commit = executeOne(dut, addi(16, 0, 1), memory)
        commit.invalid mustBe true
        commit.halt mustBe true
        commit.gprs(0) mustBe 0
        dut.io.retireValid.peek().litToBoolean mustBe false
      }
    }
  }
}
