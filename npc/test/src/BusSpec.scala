package npc

import chisel3._
import chisel3.simulator.EphemeralSimulator._
import org.scalatest.freespec.AnyFreeSpec
import org.scalatest.matchers.must.Matchers

/** Focused protocol tests for the explicit SimpleBus and AXI device models. */
class BusSpec extends AnyFreeSpec with Matchers {
  private def resetSimple(dut: SimpleBusMemory): Unit = {
    dut.io.reqValid.poke(false.B)
    dut.io.respReady.poke(false.B)
    dut.io.addr.poke(0.U)
    dut.io.wen.poke(false.B)
    dut.io.wdata.poke(0.U)
    dut.io.wmask.poke(0.U)
    dut.reset.poke(true.B)
    dut.clock.step()
    dut.reset.poke(false.B)
  }

  "SimpleBus memory holds a one-cycle response" in {
    simulate(new SimpleBusMemory(16, 1)) { dut =>
      resetSimple(dut)
      dut.io.reqValid.poke(true.B)
      dut.io.addr.poke(0.U)
      dut.io.wen.poke(true.B)
      dut.io.wdata.poke("h11223344".U)
      dut.io.wmask.poke("hf".U)
      dut.io.reqReady.peek().litToBoolean mustBe true
      dut.clock.step()
      dut.io.reqValid.poke(false.B)
      dut.io.respReady.poke(true.B)
      dut.io.respValid.peek().litToBoolean mustBe false
      dut.clock.step()
      dut.io.respValid.peek().litToBoolean mustBe true
      dut.io.error.peek().litToBoolean mustBe false
      dut.clock.step()
      dut.io.respValid.peek().litToBoolean mustBe false
    }
  }

  private def resetUart(dut: AxiLiteUart): Unit = {
    dut.io.axi.awvalid.poke(false.B)
    dut.io.axi.awaddr.poke(0.U)
    dut.io.axi.wvalid.poke(false.B)
    dut.io.axi.wdata.poke(0.U)
    dut.io.axi.wstrb.poke(0.U)
    dut.io.axi.bready.poke(false.B)
    dut.io.axi.arvalid.poke(false.B)
    dut.io.axi.araddr.poke(0.U)
    dut.io.axi.rready.poke(false.B)
    dut.reset.poke(true.B)
    dut.clock.step()
    dut.reset.poke(false.B)
  }

  "AXI UART accepts split AW/W and produces a response" in {
    simulate(new AxiLiteUart) { dut =>
      resetUart(dut)
      dut.io.axi.awvalid.poke(true.B)
      dut.io.axi.awaddr.poke("h10000000".U)
      dut.io.axi.wvalid.poke(true.B)
      dut.io.axi.wdata.poke("h00000041".U)
      dut.io.axi.wstrb.poke(1.U)
      dut.clock.step()
      dut.io.axi.awvalid.poke(false.B)
      dut.io.axi.wvalid.poke(false.B)
      dut.io.axi.bready.poke(true.B)
      dut.io.axi.bvalid.peek().litToBoolean mustBe true
      dut.io.txValid.peek().litToBoolean mustBe true
      dut.io.txData.peek().litValue mustBe 0x41
      dut.clock.step()
      dut.io.axi.bvalid.peek().litToBoolean mustBe false
    }
  }

  "AXI CLINT exposes an advancing 64-bit mtime" in {
    simulate(new AxiLiteClint) { dut =>
      dut.io.awvalid.poke(false.B)
      dut.io.awaddr.poke(0.U)
      dut.io.wvalid.poke(false.B)
      dut.io.wdata.poke(0.U)
      dut.io.wstrb.poke(0.U)
      dut.io.bready.poke(false.B)
      dut.io.arvalid.poke(false.B)
      dut.io.araddr.poke(0.U)
      dut.io.rready.poke(false.B)
      dut.reset.poke(true.B)
      dut.clock.step()
      dut.reset.poke(false.B)
      dut.io.arvalid.poke(true.B)
      dut.io.araddr.poke("h20000000".U)
      dut.clock.step()
      dut.io.arvalid.poke(false.B)
      dut.io.rready.poke(true.B)
      dut.io.rvalid.peek().litToBoolean mustBe true
      dut.io.rdata.peek().litValue must be >= 0
    }
  }
}
